/**
 * Depending on the type of package, there are different
 * compilation rules for this directory.  This comment applies
 * to packages of type "pkg."  For other types of packages,
 * please view the documentation at http://mynewt.apache.org/.
 *
 * Put source files in this directory.  All files that have a *.c
 * ending are recursively compiled in the src/ directory and its
 * descendants.  The exception here is the arch/ directory, which
 * is ignored in the default compilation.
 *
 * The arch/<your-arch>/ directories are manually added and
 * recursively compiled for all files that end with either *.c
 * or *.a.  Any directories in arch/ that don't match the
 * architecture being compiled are not compiled.
 *
 * Architecture is set by the BSP/MCU combination.
 */

#include <nimble/hci_common.h>
#include "sysinit/sysinit.h"
#include <stdio.h>
#include "host/ble_gap.h"
#include "bsp/bsp.h"
#include "eventdispatcher/eventdispatcher.h"
#include "os/os.h"
#include "console/console.h"
#include "log/log.h"
#include "ble_observer/ble_observer.h"

struct ble_gap_ext_disc_params disc_params;

void
print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    console_printf("%02x:%02x:%02x:%02x:%02x:%02x",
                u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

static void
ble_observer_decode_adv_data(uint8_t *adv_data, uint8_t adv_data_len)
{
    //struct ble_hs_adv_fields fields;

    console_printf(" length_data=%d data=", adv_data_len);
    //print_bytes(adv_data, adv_data_len);
    for (int i = 0; i < adv_data_len; i++) {
        console_printf("%02x ", adv_data[i]);
    }
    console_printf("\n");
}

static void
ble_observer_decode_event_type(struct ble_gap_ext_disc_desc *desc)
{
    uint8_t directed = 0;

    if (desc->props & BLE_HCI_ADV_LEGACY_MASK) {
       return;
    }

    console_printf("Extended adv: ");
    if (desc->props & BLE_HCI_ADV_CONN_MASK) {
        console_printf("'conn' ");
    }
    if (desc->props & BLE_HCI_ADV_SCAN_MASK) {
        console_printf("'scan' ");
    }
    if (desc->props & BLE_HCI_ADV_DIRECT_MASK) {
        console_printf("'dir' ");
        directed = 1;
    }

    if (desc->props & BLE_HCI_ADV_SCAN_RSP_MASK) {
        console_printf("'scan rsp' ");
    }

    switch(desc->data_status) {
        case BLE_HCI_ADV_DATA_STATUS_COMPLETE:
            console_printf("completed");
            break;
        case BLE_HCI_ADV_DATA_STATUS_INCOMPLETE:
            console_printf("incompleted");
            break;
        case BLE_HCI_ADV_DATA_STATUS_TRUNCATED:
            console_printf("truncated");
            break;
        case BLE_HCI_ADV_DATA_STATUS_MASK:
            console_printf("mask");
            break;
        default:
            console_printf("reserved %d", desc->data_status);
            break;
    }

    console_printf(" rssi=%d txpower=%d, pphy=%d, sphy=%d, sid=%d,"
                           " addr_type=%d addr=",
                   desc->rssi, desc->tx_power, desc->prim_phy, desc->sec_phy,
                   desc->sid, desc->addr.type);
    print_addr(desc->addr.val);
    if (directed) {
        console_printf(" init_addr_type=%d inita=", desc->direct_addr.type);
        print_addr(desc->direct_addr.val);
    }

    console_printf("\n");

    if(!desc->length_data) {
        return;
    }

    ble_observer_decode_adv_data(desc->data, desc->length_data);
}

static int
ble_observer_gap_event(struct ble_gap_event *event, void *arg) {
//    struct ble_gap_conn_desc desc;
//    int conn_idx;
//    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_EXT_DISC: {
            event_dispatcher_evt_t* evt = event_dispatcher_create_event(EVENT_DISPATCHER_BLE_ADV);
            if (evt == NULL) {
                return -1;
            }
            evt->event.ble_hci_sync.sync_state = true;
            event_dispatcher_put(evt);

            ble_observer_decode_event_type(&event->ext_disc);

            return 0;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
            console_printf("disc complete\n");
            return 0;
    }
    return 0;

}

// subscribe when on_sync occurs
static void on_ble_sync(event_dispatcher_evt_t* evt) {
    int rc;
    struct ble_gap_ext_disc_params uncoded = {0};

    uncoded.window = disc_params.window;
    uncoded.itvl = disc_params.itvl;
    uncoded.passive = true;

    console_printf("on_ble_sync in ble_observer");
    uint8_t own_addr_type;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        console_printf("error determining address type; rc=%d\n", rc);
        return;
    }

    rc = ble_gap_ext_disc(own_addr_type, disc_params.window, disc_params.window, 0, BLE_HCI_SCAN_FILT_NO_WL, 0, &uncoded, NULL, ble_observer_gap_event, NULL);
    assert(rc == 0);
}

void ble_observer_init() {
    disc_params.itvl = 0xa0;
    disc_params.window = 0x50;
    disc_params.passive = true;

    event_dispatcher_subscribe(EVENT_DISPATCHER_BLE_HCI_SYNC, on_ble_sync);

}