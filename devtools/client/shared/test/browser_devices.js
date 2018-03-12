/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  getDevices,
  getDeviceString,
  addDevice
} = require("devtools/client/shared/devices");

add_task(function* () {
  Services.prefs.setCharPref("devtools.devices.url",
                             TEST_URI_ROOT + "browser_devices.json");

  let devices = yield getDevices();

  is(devices.TYPES.length, 1, "Found 1 device type.");

  let type1 = devices.TYPES[0];

  is(devices[type1].length, 2, "Found 2 devices of type #1.");

  let string = getDeviceString(type1);
  ok(typeof string === "string" && string.length > 0, "Able to localize type #1.");

  let device1 = {
    name: "SquarePhone",
    width: 320,
    height: 320,
    pixelRatio: 2,
    userAgent: "Mozilla/5.0 (Mobile; rv:42.0)",
    touch: true,
    firefoxOS: true
  };
  addDevice(device1, type1);
  devices = yield getDevices();

  is(devices[type1].length, 3, "Added new device of type #1.");
  ok(devices[type1].filter(d => d.name === device1.name), "Found the new device.");

  let type2 = "appliances";
  let device2 = {
    name: "Mr Freezer",
    width: 800,
    height: 600,
    pixelRatio: 5,
    userAgent: "Mozilla/5.0 (Appliance; rv:42.0)",
    touch: true,
    firefoxOS: true
  };
  addDevice(device2, type2);
  devices = yield getDevices();

  is(devices.TYPES.length, 2, "Added device type #2.");
  is(devices[type2].length, 1, "Added new device of type #2.");
});
