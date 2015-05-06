/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _BTLE_DISCOVERY_H_
#define _BTLE_DISCOVERY_H_

#include "blecommon.h"
#include "UUID.h"
#include "Gap.h"
#include "ble_gattc.h"
#include <stdio.h>

/**@brief Structure for holding information about the service and the characteristics found during
 *        the discovery process.
 */
struct DiscoveredService {
    void setup(ShortUUIDBytes_t uuidIn, Gap::Handle_t start, Gap::Handle_t end) {
        uuid        = uuidIn;
        startHandle = start;
        endHandle   = end;
    }

    ShortUUIDBytes_t uuid;        /**< UUID of the service. */
    Gap::Handle_t    startHandle; /**< Service Handle Range. */
    Gap::Handle_t    endHandle;   /**< Service Handle Range. */
};

/**@brief Structure for holding information about the service and the characteristics found during
 *        the discovery process.
 */
struct DiscoveredCharacteristic {
    struct Properties_t {
        static const uint8_t BROADCAST_PROPERTY_MASK         = 0x01;
        static const uint8_t READ_PROPERTY_MASK              = 0x02;
        static const uint8_t WRITE_WO_RESPONSE_PROPERTY_MASK = 0x04;
        static const uint8_t WRITE_PROPERTY_MASK             = 0x08;
        static const uint8_t NOTIFY_PROPERTY_MASK            = 0x10;
        static const uint8_t INDICATE_PROPERTY_MASK          = 0x20;
        static const uint8_t AUTH_SIGNED_PROPERTY_MASK       = 0x40;

        Properties_t() : broadcast(0), read(0), write_wo_resp(0), write(0), notify(0), indicate(0), auth_signed_wr(0) {
            /* empty */
        }

        Properties_t(uint8_t props) :
            broadcast(props & BROADCAST_PROPERTY_MASK),
            read(props & READ_PROPERTY_MASK),
            write_wo_resp(props & WRITE_WO_RESPONSE_PROPERTY_MASK),
            write(props & WRITE_PROPERTY_MASK),
            notify(props & NOTIFY_PROPERTY_MASK),
            indicate(props & INDICATE_PROPERTY_MASK),
            auth_signed_wr(props & AUTH_SIGNED_PROPERTY_MASK) {
            /* empty*/
        }

        uint8_t broadcast       :1; /**< Broadcasting of the value permitted. */
        uint8_t read            :1; /**< Reading the value permitted. */
        uint8_t write_wo_resp   :1; /**< Writing the value with Write Command permitted. */
        uint8_t write           :1; /**< Writing the value with Write Request permitted. */
        uint8_t notify          :1; /**< Notications of the value permitted. */
        uint8_t indicate        :1; /**< Indications of the value permitted. */
        uint8_t auth_signed_wr  :1; /**< Writing the value with Signed Write Command permitted. */
    };

    void setup(ShortUUIDBytes_t uuidIn, Properties_t propsIn, Gap::Handle_t declHandleIn, Gap::Handle_t valueHandleIn) {
        uuid        = uuidIn;
        props       = propsIn;
        declHandle  = declHandleIn;
        valueHandle = valueHandleIn;
    }

    ShortUUIDBytes_t uuid;
    Properties_t     props;
    Gap::Handle_t    declHandle;
    Gap::Handle_t    valueHandle;
};

class ServiceDiscovery {
public:
    static const unsigned BLE_DB_DISCOVERY_MAX_SRV           = 4;  /**< Maximum number of services supported by this module. This also indicates the maximum number of users allowed to be registered to this module. (one user per service). */
    static const unsigned BLE_DB_DISCOVERY_MAX_CHAR_PER_SRV  = 4;  /**< Maximum number of characteristics per service supported by this module. */

    static const uint16_t SRV_DISC_START_HANDLE              = 0x0001; /**< The start handle value used during service discovery. */

    typedef void (*ServiceCallback_t)(void);
    typedef void (*CharacteristicCallback_t)(void);

public:
    static ble_error_t launch(Gap::Handle_t            connectionHandle,
                              ServiceCallback_t        sc,
                              CharacteristicCallback_t cc = NULL);
    static ble_error_t launch(Gap::Handle_t            connectionHandle,
                              UUID                     matchingServiceUUID,
                              ServiceCallback_t        sc,
                              UUID                     matchingCharacteristicUUID = ShortUUIDBytes_t(BLE_UUID_UNKNOWN),
                              CharacteristicCallback_t cc = NULL);

    static ServiceDiscovery *getSingleton(void);

private:
    ble_error_t launchCharacteristicDiscovery(Gap::Handle_t connectionHandle, Gap::Handle_t startHandle, Gap::Handle_t endHandle);

public:
    void terminate(void) {
        serviceDiscoveryInProgress = false;
        printf("end of service discovery\r\n");
    }

    void terminateCharacteristicDiscovery(void) {
        characteristicDiscoveryInProgress = false;
        serviceDiscoveryInProgress        = true;
        currSrvInd++; /* Progress service index to keep discovery alive. */
    }

    void resetDiscoveredServices(void) {
        srvCount   = 0;
        currSrvInd = 0;
        memset(services, 0, sizeof(DiscoveredService) * BLE_DB_DISCOVERY_MAX_SRV);
    }

    void resetDiscoveredCharacteristics(void) {
        charCount   = 0;
        currCharInd = 0;
        memset(characteristics, 0, sizeof(DiscoveredCharacteristic) * BLE_DB_DISCOVERY_MAX_CHAR_PER_SRV);
    }

    void setupDiscoveredServices(const ble_gattc_evt_prim_srvc_disc_rsp_t *response);
    void setupDiscoveredCharacteristics(const ble_gattc_evt_char_disc_rsp_t *response);

    void progressCharacteristicDiscovery() {
        while (characteristicDiscoveryInProgress && (currCharInd < charCount)) {
            /* THIS IS WHERE THE CALLBACK WILL GO */
            printf("%x [%u]\r\n", characteristics[currCharInd].uuid, characteristics[currCharInd].valueHandle);

            currCharInd++;
        }

        if (characteristicDiscoveryInProgress) {
            Gap::Handle_t startHandle = characteristics[currCharInd - 1].valueHandle + 1;
            Gap::Handle_t endHandle   = services[currSrvInd].endHandle;
            resetDiscoveredCharacteristics();

            if (startHandle < endHandle) {
                ble_gattc_handle_range_t handleRange = {
                    .start_handle = startHandle,
                    .end_handle   = endHandle
                };
                printf("char discovery returned %u\r\n", sd_ble_gattc_characteristics_discover(connHandle, &handleRange));
            } else {
               terminateCharacteristicDiscovery();
            }
        }
    }

    void progressServiceDiscovery() {
        while (serviceDiscoveryInProgress && (currSrvInd < srvCount)) {
            /* THIS IS WHERE THE CALLBACK WILL GO */
            printf("%x [%u %u]\r\n", services[currSrvInd].uuid, services[currSrvInd].startHandle, services[currSrvInd].endHandle);

            if (true) { /* characteristic discovery is optional. */
                launchCharacteristicDiscovery(connHandle, services[currSrvInd].startHandle, services[currSrvInd].endHandle);
            } else {
                currSrvInd++; /* Progress service index to keep discovery alive. */
            }
        }
        if (serviceDiscoveryInProgress && (srvCount > 0) && (currSrvInd > 0)) {
            Gap::Handle_t endHandle = services[currSrvInd - 1].endHandle;
            resetDiscoveredServices();

            printf("services discover returned %u\r\n", sd_ble_gattc_primary_services_discover(connHandle, endHandle, NULL));
        }
    }

    void serviceDiscoveryStarted(Gap::Handle_t connectionHandle) {
        connHandle                        = connectionHandle;
        resetDiscoveredServices();
        serviceDiscoveryInProgress        = true;
        characteristicDiscoveryInProgress = false;
    }

private:
    void characteristicDiscoveryStarted(Gap::Handle_t connectionHandle) {
        connHandle                        = connectionHandle;
        resetDiscoveredCharacteristics();
        characteristicDiscoveryInProgress = true;
        serviceDiscoveryInProgress        = false;
    }

private:
    ServiceDiscovery() {
        /* empty */
    }

public:

    DiscoveredService        services[BLE_DB_DISCOVERY_MAX_SRV];  /**< Information related to the current service being discovered.
                                                                   *  This is intended for internal use during service discovery. */
    DiscoveredCharacteristic characteristics[BLE_DB_DISCOVERY_MAX_CHAR_PER_SRV];

    uint16_t connHandle;  /**< Connection handle as provided by the SoftDevice. */
    uint8_t  currSrvInd;  /**< Index of the current service being discovered. This is intended for internal use during service discovery.*/
    uint8_t  srvCount;    /**< Number of services at the peers GATT database.*/
    uint8_t  currCharInd; /**< Index of the current characteristic being discovered. This is intended for internal use during service discovery.*/
    uint8_t  charCount;    /**< Number of characteristics within the service.*/

    bool     serviceDiscoveryInProgress;
    bool     characteristicDiscoveryInProgress;
};

#endif /*_BTLE_DISCOVERY_H_*/
