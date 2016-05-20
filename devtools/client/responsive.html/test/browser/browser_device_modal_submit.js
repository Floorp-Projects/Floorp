/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test submitting display device changes on the device modal

const TEST_URL = "data:text/html;charset=utf-8,";

addRDMTask(TEST_URL, function* ({ ui }) {
  let { store, document } = ui.toolWindow;
  let modal = document.querySelector(".device-modal");
  let select = document.querySelector(".viewport-device-selector");
  let submitButton = document.querySelector("#device-submit-button");

  // Wait until the viewport has been added
  yield waitUntilState(store, state => state.viewports.length == 1);

  openDeviceModal(ui);

  info("Checking displayed device checkboxes are checked in the device modal.");
  let checkedCbs = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => cb.checked);
  let deviceList = loadDeviceList();

  is(deviceList.length, checkedCbs.length,
    "Got expected number of displayed devices.");

  for (let cb of checkedCbs) {
    ok(deviceList.includes(cb.value), cb.value + " is correctly checked.");
  }

  info("Check the first unchecked device and submit new device list.");
  let uncheckedCb = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => !cb.checked)[0];
  let value = uncheckedCb.value;
  uncheckedCb.click();
  submitButton.click();

  ok(modal.classList.contains("hidden"),
    "The device modal is hidden on submit.");

  info("Checking new device is added to the displayed device list.");
  deviceList = loadDeviceList();
  ok(deviceList.includes(value), value + " added to displayed device list.");

  info("Checking new device is added to the device selector.");
  let options = [...select.options];
  is(options.length - 2, deviceList.length,
    "Got expected number of devices in device selector.");
  ok(options.filter(o => o.value === value)[0],
    value + " added to the device selector.");

  info("Reopen device modal and check new device is correctly checked");
  openDeviceModal(ui);
  ok([...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => cb.checked && cb.value === value)[0],
    value + " is checked in the device modal.");
});
