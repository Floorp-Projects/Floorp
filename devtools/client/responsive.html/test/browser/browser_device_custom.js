/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test adding and removing custom devices via the modal.

const device = {
  name: "Test Device",
  width: 400,
  height: 570,
  pixelRatio: 1.5,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: true,
};

const unicodeDevice = {
  name: "\u00B6\u00C7\u00DA\u00E7\u0126",
  width: 400,
  height: 570,
  pixelRatio: 1.5,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: true,
};

const TEST_URL = "data:text/html;charset=utf-8,";
const Types = require("devtools/client/responsive.html/types");

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { store, document } = toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  const deviceSelector = document.querySelector(".viewport-device-selector");
  const submitButton = document.querySelector("#device-submit-button");

  openDeviceModal(ui);

  info("Reveal device adder form, check that defaults match the viewport");
  const adderShow = document.querySelector("#device-adder-show");
  adderShow.click();
  testDeviceAdder(ui, {
    name: "Custom Device",
    width: 320,
    height: 480,
    pixelRatio: window.devicePixelRatio,
    userAgent: navigator.userAgent,
    touch: false,
  });

  info("Fill out device adder form and save");
  await addDeviceInModal(ui, device);

  info("Verify device defaults to enabled in modal");
  const deviceCb = [...document.querySelectorAll(".device-input-checkbox")].find(cb => {
    return cb.value == device.name;
  });
  ok(deviceCb, "Custom device checkbox added to modal");
  ok(deviceCb.checked, "Custom device enabled");
  submitButton.click();

  info("Look for custom device in device selector");
  const selectorOption =
    [...deviceSelector.options].find(opt => opt.value == device.name);
  ok(selectorOption, "Custom device option added to device selector");
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { store, document } = toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  const deviceSelector = document.querySelector(".viewport-device-selector");
  const submitButton = document.querySelector("#device-submit-button");

  info("Select existing device from the selector");
  await selectDevice(ui, "Test Device");

  openDeviceModal(ui);

  info("Reveal device adder form, check that defaults are based on selected device");
  const adderShow = document.querySelector("#device-adder-show");
  adderShow.click();
  testDeviceAdder(ui, Object.assign({}, device, {
    name: "Test Device (Custom)",
  }));

  info("Remove previously added custom device");
  const deviceRemoveButton = document.querySelector(".device-remove-button");
  const removed = Promise.all([
    waitUntilState(store, state => state.devices.custom.length == 0),
    once(ui, "device-association-removed")
  ]);
  deviceRemoveButton.click();
  await removed;
  submitButton.click();

  info("Ensure custom device was removed from device selector");
  await waitUntilState(store, state => state.viewports[0].device == "");
  is(deviceSelector.value, "", "Device selector reset to no device");
  const selectorOption =
    [...deviceSelector.options].find(opt => opt.value == device.name);
  ok(!selectorOption, "Custom device option removed from device selector");

  info("Ensure device properties like UA have been reset");
  await testUserAgent(ui, navigator.userAgent);
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { store, document } = toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  const deviceSelector = document.querySelector(".viewport-device-selector");
  const submitButton = document.querySelector("#device-submit-button");

  openDeviceModal(ui);

  info("Reveal device adder form");
  const adderShow = document.querySelector("#device-adder-show");
  adderShow.click();

  info("Fill out device adder form by setting details to unicode device and save");
  await addDeviceInModal(ui, unicodeDevice);

  info("Verify unicode device defaults to enabled in modal");
  const deviceCb = [...document.querySelectorAll(".device-input-checkbox")].find(cb => {
    return cb.value == unicodeDevice.name;
  });
  ok(deviceCb, "Custom unicode device checkbox added to modal");
  ok(deviceCb.checked, "Custom unicode device enabled");
  submitButton.click();

  info("Look for custom unicode device in device selector");
  const selectorOption = [...deviceSelector.options].find(opt =>
    opt.value == unicodeDevice.name);
  ok(selectorOption, "Custom unicode device option added to device selector");
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { store, document } = toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  const deviceSelector = document.querySelector(".viewport-device-selector");

  // Check if the unicode custom device is present in the list of device options since
  // we want to ensure that unicode device names are not forgotten after restarting RDM
  // see bug 1379687
  info("Look for custom unicode device in device selector");
  const selectorOption = [...deviceSelector.options].find(opt =>
    opt.value == unicodeDevice.name);
  ok(selectorOption, "Custom unicode device option present in device selector");
});

function testDeviceAdder(ui, expected) {
  const { document } = ui.toolWindow;

  const nameInput = document.querySelector("#device-adder-name input");
  const [ widthInput, heightInput ] =
    document.querySelectorAll("#device-adder-size input");
  const pixelRatioInput = document.querySelector("#device-adder-pixel-ratio input");
  const userAgentInput = document.querySelector("#device-adder-user-agent input");
  const touchInput = document.querySelector("#device-adder-touch input");

  is(nameInput.value, expected.name, "Device name matches");
  is(parseInt(widthInput.value, 10), expected.width, "Width matches");
  is(parseInt(heightInput.value, 10), expected.height, "Height matches");
  is(parseFloat(pixelRatioInput.value), expected.pixelRatio,
     "devicePixelRatio matches");
  is(userAgentInput.value, expected.userAgent, "User agent matches");
  is(touchInput.checked, expected.touch, "Touch matches");
}
