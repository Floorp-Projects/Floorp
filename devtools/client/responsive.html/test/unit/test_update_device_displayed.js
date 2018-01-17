/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test updating the device `displayed` property

const {
  addDevice,
  addDeviceType,
  updateDeviceDisplayed,
} = require("devtools/client/responsive.html/actions/devices");

add_task(async function () {
  let store = Store();
  const { getState, dispatch } = store;

  let device = {
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
  dispatch(updateDeviceDisplayed(device, "phones", true));

  equal(getState().devices.phones.length, 1,
    "Correct number of phones");
  ok(getState().devices.phones[0].displayed,
    "Device phone list contains enabled Firefox OS Flame");
});
