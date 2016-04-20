/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test submitting display device changes on the device modal

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, function* ({ ui }) {
  let { store, document } = ui.toolWindow;
  let modal = document.querySelector(".device-modal");
  let closeButton = document.querySelector("#device-close-button");

  // Wait until the viewport has been added
  yield waitUntilState(store, state => state.viewports.length == 1);

  openDeviceModal(ui);

  let deviceListBefore = loadDeviceList();

  info("Check the first unchecked device and exit the modal.");
  let uncheckedCb = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => !cb.checked)[0];
  let value = uncheckedCb.value;
  uncheckedCb.click();
  closeButton.click();

  ok(modal.classList.contains("hidden"),
    "The device modal is hidden on exit.");

  info("Check that the device list remains unchanged after exitting.");
  let deviceListAfter = loadDeviceList();
  is(deviceListBefore.length, deviceListAfter.length,
    "Got expected number of displayed devices.");
  ok(!deviceListAfter.includes(value),
    value + " was not added to displayed device list.");
});
