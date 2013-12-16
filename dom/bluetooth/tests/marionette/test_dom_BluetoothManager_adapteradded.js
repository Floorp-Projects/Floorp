/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

// https://github.com/mozilla-b2g/platform_external_qemu/blob/master/vl-android.c#L765
// static int bt_hci_parse(const char *str) {
//   ...
//   bdaddr.b[0] = 0x52;
//   bdaddr.b[1] = 0x54;
//   bdaddr.b[2] = 0x00;
//   bdaddr.b[3] = 0x12;
//   bdaddr.b[4] = 0x34;
//   bdaddr.b[5] = 0x56 + nb_hcis;
const EMULATOR_ADDRESS = "56:34:12:00:54:52";

// $ adb shell hciconfig /dev/ttyS2 name
// hci0:  Type: BR/EDR  Bus: UART
//        BD Address: 56:34:12:00:54:52  ACL MTU: 512:1  SCO MTU: 0:0
//        Name: 'Full Android on Emulator'
const EMULATOR_NAME = "Full Android on Emulator";

// $ adb shell hciconfig /dev/ttyS2 class
// hci0:  Type: BR/EDR  Bus: UART
//        BD Address: 56:34:12:00:54:52  ACL MTU: 512:1  SCO MTU: 0:0
//        Class: 0x58020c
//        Service Classes: Capturing, Object Transfer, Telephony
//        Device Class: Phone, Smart phone
const EMULATOR_CLASS = 0x58020c;

startBluetoothTest(true, function testCaseMain(aAdapter) {
  log("Checking adapter attributes ...");

  is(aAdapter.name, EMULATOR_NAME, "adapter.name");
  is(aAdapter.class, EMULATOR_CLASS, "adapter.class");
  is(aAdapter.address, EMULATOR_ADDRESS, "adapter.address");
  is(aAdapter.discovering, false, "adapter.discovering");
  is(aAdapter.discoverable, false, "adapter.discoverable");
  is(aAdapter.discoverableTimeout, 120, "adapter.discoverableTimeout");
});
