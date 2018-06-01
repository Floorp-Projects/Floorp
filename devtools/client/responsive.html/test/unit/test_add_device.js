/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding a new device.

const {
  addDevice,
  addDeviceType,
} = require("devtools/client/responsive.html/actions/devices");

add_task(async function() {
  const store = Store();
  const { getState, dispatch } = store;

  const device = {
    "name": "Firefox OS Flame",
    "width": 320,
    "height": 570,
    "pixelRatio": 1.5,
    "userAgent": "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
    "touch": true,
    "firefoxOS": true,
    "os": "fxos"
  };

  dispatch(addDeviceType("phones"));
  dispatch(addDevice(device, "phones"));

  equal(getState().devices.phones.length, 1,
    "Correct number of phones");
  ok(getState().devices.phones.includes(device),
    "Device phone list contains Firefox OS Flame");
});
