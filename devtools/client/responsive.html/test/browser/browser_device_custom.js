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
  firefoxOS: false,
  os: "android",
};

const TEST_URL = "data:text/html;charset=utf-8,";
const Types = require("devtools/client/responsive.html/types");

addRDMTask(TEST_URL, function* ({ ui }) {
  let { toolWindow } = ui;
  let { store, document } = toolWindow;
  let React = toolWindow.require("devtools/client/shared/vendor/react");
  let { Simulate } = React.addons.TestUtils;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  let deviceSelector = document.querySelector(".viewport-device-selector");
  let submitButton = document.querySelector("#device-submit-button");

  openDeviceModal(ui);

  info("Reveal device adder form, check that defaults match the viewport");
  let adderShow = document.querySelector("#device-adder-show");
  Simulate.click(adderShow);
  testDeviceAdder(ui, {
    name: "Custom Device",
    width: 320,
    height: 480,
    pixelRatio: window.devicePixelRatio,
    userAgent: navigator.userAgent,
    touch: false,
  });

  info("Fill out device adder form and save");
  setDeviceAdder(ui, device);
  let adderSave = document.querySelector("#device-adder-save");
  let saved = waitUntilState(store, state => state.devices.custom.length == 1);
  Simulate.click(adderSave);
  yield saved;

  info("Enable device in modal");
  let deviceCb = [...document.querySelectorAll(".device-input-checkbox")].find(cb => {
    return cb.value == device.name;
  });
  ok(deviceCb, "Custom device checkbox added to modal");
  deviceCb.click();
  Simulate.click(submitButton);

  info("Look for custom device in device selector");
  let selectorOption = [...deviceSelector.options].find(opt => opt.value == device.name);
  ok(selectorOption, "Custom device option added to device selector");
});

addRDMTask(TEST_URL, function* ({ ui }) {
  let { toolWindow } = ui;
  let { store, document } = toolWindow;
  let React = toolWindow.require("devtools/client/shared/vendor/react");
  let { Simulate } = React.addons.TestUtils;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  let deviceSelector = document.querySelector(".viewport-device-selector");
  let submitButton = document.querySelector("#device-submit-button");

  info("Select existing device from the selector");
  yield selectDevice(ui, "Test Device");

  openDeviceModal(ui);

  info("Reveal device adder form, check that defaults are based on selected device");
  let adderShow = document.querySelector("#device-adder-show");
  Simulate.click(adderShow);
  testDeviceAdder(ui, Object.assign({}, device, {
    name: "Test Device (Custom)",
  }));

  info("Remove previously added custom device");
  let deviceRemoveButton = document.querySelector(".device-remove-button");
  let removed = Promise.all([
    waitUntilState(store, state => state.devices.custom.length == 0),
    once(ui, "device-removed")
  ]);
  Simulate.click(deviceRemoveButton);
  yield removed;
  Simulate.click(submitButton);

  info("Ensure custom device was removed from device selector");
  yield waitUntilState(store, state => state.viewports[0].device == "");
  is(deviceSelector.value, "", "Device selector reset to no device");
  let selectorOption = [...deviceSelector.options].find(opt => opt.value == device.name);
  ok(!selectorOption, "Custom device option removed from device selector");

  info("Ensure device properties like UA have been reset");
  yield testUserAgent(ui, navigator.userAgent);
});

function testDeviceAdder(ui, expected) {
  let { document } = ui.toolWindow;

  let nameInput = document.querySelector("#device-adder-name input");
  let [ widthInput, heightInput ] = document.querySelectorAll("#device-adder-size input");
  let pixelRatioInput = document.querySelector("#device-adder-pixel-ratio input");
  let userAgentInput = document.querySelector("#device-adder-user-agent input");
  let touchInput = document.querySelector("#device-adder-touch input");

  is(nameInput.value, expected.name, "Device name matches");
  is(parseInt(widthInput.value, 10), expected.width, "Width matches");
  is(parseInt(heightInput.value, 10), expected.height, "Height matches");
  is(parseFloat(pixelRatioInput.value), expected.pixelRatio,
     "devicePixelRatio matches");
  is(userAgentInput.value, expected.userAgent, "User agent matches");
  is(touchInput.checked, expected.touch, "Touch matches");
}

function setDeviceAdder(ui, value) {
  let { toolWindow } = ui;
  let { document } = ui.toolWindow;
  let React = toolWindow.require("devtools/client/shared/vendor/react");
  let { Simulate } = React.addons.TestUtils;

  let nameInput = document.querySelector("#device-adder-name input");
  let [ widthInput, heightInput ] = document.querySelectorAll("#device-adder-size input");
  let pixelRatioInput = document.querySelector("#device-adder-pixel-ratio input");
  let userAgentInput = document.querySelector("#device-adder-user-agent input");
  let touchInput = document.querySelector("#device-adder-touch input");

  nameInput.value = value.name;
  Simulate.change(nameInput);
  widthInput.value = value.width;
  Simulate.change(widthInput);
  Simulate.blur(widthInput);
  heightInput.value = value.height;
  Simulate.change(heightInput);
  Simulate.blur(heightInput);
  pixelRatioInput.value = value.pixelRatio;
  Simulate.change(pixelRatioInput);
  userAgentInput.value = value.userAgent;
  Simulate.change(userAgentInput);
  touchInput.checked = value.touch;
  Simulate.change(touchInput);
}
