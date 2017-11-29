/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding several devices and removing one to ensure the correct device is removed.

const TEST_URL = "data:text/html;charset=utf-8,";
const Types = require("devtools/client/responsive.html/types");

const device = {
  width: 400,
  height: 570,
  pixelRatio: 1.5,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: true,
};

const device1 = Object.assign({}, device, {
  name: "Test Device 1",
});

const device2 = Object.assign({}, device, {
  name: "Test Device 2",
});

addRDMTask(TEST_URL, function* ({ ui }) {
  let { toolWindow } = ui;
  let { store, document } = toolWindow;

  info("Verify that remove buttons affect the correct device");

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  let deviceSelector = document.querySelector(".viewport-device-selector");
  let submitButton = document.querySelector("#device-submit-button");

  openDeviceModal(ui);

  info("Reveal device adder form");
  let adderShow = document.querySelector("#device-adder-show");
  adderShow.click();

  info("Add test device 1");
  yield addDeviceInModal(ui, device1);

  info("Reveal device adder form");
  adderShow = document.querySelector("#device-adder-show");
  adderShow.click();

  info("Add test device 2");
  yield addDeviceInModal(ui, device2);

  info("Verify all custom devices default to enabled in modal");
  let deviceCbs =
    [...document.querySelectorAll(".device-type-custom .device-input-checkbox")];
  is(deviceCbs.length, 2, "Both devices have a checkbox in modal");
  for (let cb of deviceCbs) {
    ok(cb.checked, "Custom device enabled");
  }
  submitButton.click();

  info("Look for device 1 in device selector");
  let deviceOption1 = [...deviceSelector.options].find(opt => opt.value == device1.name);
  ok(deviceOption1, "Test device 1 option added to device selector");

  info("Look for device 2 in device selector");
  let deviceOption2 = [...deviceSelector.options].find(opt => opt.value == device2.name);
  ok(deviceOption2, "Test device 2 option added to device selector");

  openDeviceModal(ui);

  info("Remove device 2");
  let deviceRemoveButtons = [...document.querySelectorAll(".device-remove-button")];
  is(deviceRemoveButtons.length, 2, "Both devices have a remove button in modal");
  let removed = waitUntilState(store, state => state.devices.custom.length == 1);
  deviceRemoveButtons[1].click();
  yield removed;
  submitButton.click();

  info("Ensure device 1 is still in device selector");
  deviceOption1 = [...deviceSelector.options].find(opt => opt.value == device1.name);
  ok(deviceOption1, "Test device 1 option exists");

  info("Ensure device 2 is no longer in device selector");
  deviceOption2 = [...deviceSelector.options].find(opt => opt.value == device2.name);
  ok(!deviceOption2, "Test device 2 option removed");
});

addRDMTask(TEST_URL, function* ({ ui }) {
  let { toolWindow } = ui;
  let { store, document } = toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  let deviceSelector = document.querySelector(".viewport-device-selector");

  info("Ensure device 1 is still in device selector");
  let deviceOption1 = [...deviceSelector.options].find(opt => opt.value == device1.name);
  ok(deviceOption1, "Test device 1 option exists");

  info("Ensure device 2 is no longer in device selector");
  let deviceOption2 = [...deviceSelector.options].find(opt => opt.value == device2.name);
  ok(!deviceOption2, "Test device 2 option removed");

  openDeviceModal(ui);

  info("Ensure device 1 is still in device modal");
  let deviceCbs =
    [...document.querySelectorAll(".device-type-custom .device-input-checkbox")];
  is(deviceCbs.length, 1, "Only 1 custom present in modal");
  let deviceCb1 = deviceCbs.find(cb => cb.value == device1.name);
  ok(deviceCb1 && deviceCb1.checked, "Test device 1 checkbox exists and enabled");

  info("Ensure device 2 is no longer in device modal");
  let deviceCb2 = deviceCbs.find(cb => cb.value == device2.name);
  ok(!deviceCb2, "Test device 2 checkbox does not exist");
});

