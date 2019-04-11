/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that adding a device, submitting it, and then editing it updates the device's
// original values when it is saved.

const TEST_URL = "data:text/html;charset=utf-8,";
const Types = require("devtools/client/responsive.html/types");

const device = {
  name: "Custom Device",
  width: 320,
  height: 480,
  pixelRatio: 1,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: false,
};

const newDevice = {
  name: "Edited Custom Device",
  width: 300,
  height: 900,
  pixelRatio: 4,
  userAgent: "Different User agent",
  touch: true,
};

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { store, document } = toolWindow;

  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);
  await openDeviceModal(ui);

  info("Add device.");
  const adderShow = document.querySelector("#device-add-button");
  adderShow.click();
  await addDeviceInModal(ui, device);

  info("Submit the added device.");
  let deviceSelector = document.getElementById("device-selector");
  document.getElementById("device-close-button").click();

  await testMenuItems(toolWindow, deviceSelector, menuItems => {
    const originalDevice = menuItems.find(i => i.getAttribute("label") === device.name);
    ok(originalDevice, "Original custom device menu item exists.");
  });

  info("Select the added device in menu.");
  await selectDevice(ui, "Custom Device");
  await openDeviceModal(ui);

  info("Edit the device.");
  const editorShow = document.querySelector(".device-type-custom #device-edit-button");
  editorShow.click();
  await editDeviceInModal(ui, device, newDevice);

  info("Ensure the edited device name is updated in the custom device list.");
  const customDevicesList = document.querySelector(".device-type-custom");
  const editedCustomDevice = customDevicesList.querySelector(".device-name");
  is(editedCustomDevice.textContent, newDevice.name,
    `${device.name} is updated to ${newDevice.name} in the custom device list`);

  info("Ensure the viewport width and height are updated in the toolbar.");
  const [ width, height ] = document
    .querySelectorAll(".text-input.viewport-dimension-input");
  is(width.value, "300", "Viewport width is 300");
  is(height.value, "900", "Viewport height is 900");

  info("Ensure the pixel ratio is updated in the toolbar.");
  const devicePixelRatioSpan = document.querySelector("#device-pixel-ratio-menu span");
  is(devicePixelRatioSpan.textContent, "DPR: 4", "Viewport pixel ratio is 4.");

  info("Ensure the user agent has been updated.");
  const userAgentInput = document.querySelector("#user-agent-input");
  is(userAgentInput.value, newDevice.userAgent,
    `Viewport user agent is ${newDevice.userAgent}`);

  info("Ensure touch simulation has been updated");
  const touchSimulation = document.querySelector("#touch-simulation-button");
  ok(touchSimulation.classList.contains("checked"),
    "Viewport touch simulation is enabled.");

  info("Ensure the edited device is updated in the device selector when submitted");
  document.getElementById("device-close-button").click();
  deviceSelector = document.getElementById("device-selector");

  await testMenuItems(toolWindow, deviceSelector, menuItems => {
    const originalDevice = menuItems.find(i => i.getAttribute("label") === device.name);
    const editedDevice = menuItems.find(i => i.getAttribute("label") === newDevice.name);
    ok(!originalDevice, "Original custom device menu item does not exist");
    ok(editedDevice, "Edited Custom Device menu item exists");
  });
});
