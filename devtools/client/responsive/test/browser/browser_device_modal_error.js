/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test to check that RDM can handle properly an error in the device list

const TEST_URL = "data:text/html;charset=utf-8,";
const Types = require("devtools/client/responsive/types");

// Set a wrong URL for the device list file
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["devtools.devices.url", TEST_URI_ROOT + "wrong_devices_file.json"]],
  });
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { store, document } = ui.toolWindow;
  const button = document.getElementById("device-selector");

  // Wait until the viewport has been added and the device list state indicates
  // an error
  await waitUntilState(
    store,
    state =>
      state.viewports.length == 1 &&
      state.devices.listState == Types.loadableState.ERROR
  );

  // The device selector should be disabled
  ok(button.disabled, "Device selector is disabled");
});
