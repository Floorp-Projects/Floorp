/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test submitting display device changes on the device modal

const TEST_URL = "data:text/html;charset=utf-8,";
const Types = require("devtools/client/responsive.html/types");

addRDMTask(TEST_URL, async function({ ui }) {
  const { store, document } = ui.toolWindow;
  const modal = document.querySelector("#device-modal-wrapper");
  const closeButton = document.querySelector("#device-close-button");

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  openDeviceModal(ui);

  const preferredDevicesBefore = _loadPreferredDevices();

  info("Check the first unchecked device and exit the modal.");
  const uncheckedCb = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => !cb.checked)[0];
  const value = uncheckedCb.value;
  uncheckedCb.click();
  closeButton.click();

  ok(modal.classList.contains("closed") && !modal.classList.contains("opened"),
    "The device modal is closed on exit.");

  info("Check that the device list remains unchanged after exitting.");
  const preferredDevicesAfter = _loadPreferredDevices();

  is(preferredDevicesBefore.added.size, preferredDevicesAfter.added.size,
    "Got expected number of added devices.");

  is(preferredDevicesBefore.removed.size, preferredDevicesAfter.removed.size,
    "Got expected number of removed devices.");

  ok(!preferredDevicesAfter.removed.has(value),
    value + " was not added to removed device list.");
});
