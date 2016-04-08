/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding a new device type.

const { addDeviceType } =
  require("devtools/client/responsive.html/actions/devices");

add_task(function* () {
  let store = Store();
  const { getState, dispatch } = store;

  dispatch(addDeviceType("phones"));

  equal(getState().devices.types.length, 1, "Correct number of device types");
  equal(getState().devices.phones.length, 0,
    "Defaults to an empty array of phones");
  ok(getState().devices.types.includes("phones"),
    "Device types contain phones");
});
