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

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { store, document } = toolWindow;

  info("Verify that remove buttons affect the correct device");

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  const deviceSelector = document.getElementById("device-selector");

  await openDeviceModal(ui);

  info("Reveal device adder form");
  let adderShow = document.querySelector("#device-add-button");
  adderShow.click();

  info("Add test device 1");
  await addDeviceInModal(ui, device1);

  info("Reveal device adder form");
  adderShow = document.querySelector("#device-add-button");
  adderShow.click();

  info("Add test device 2");
  await addDeviceInModal(ui, device2);

  info("Verify all custom devices default to enabled in modal");
  const deviceCbs =
    [...document.querySelectorAll(".device-type-custom .device-input-checkbox")];
  is(deviceCbs.length, 2, "Both devices have a checkbox in modal");
  for (const cb of deviceCbs) {
    ok(cb.checked, "Custom device enabled");
  }
  document.getElementById("device-close-button").click();

  info("Look for device 1 and 2 in device selector");

  await testMenuItems(toolWindow, deviceSelector, menuItems => {
    const deviceItem1 = menuItems.find(i => i.getAttribute("label") === device1.name);
    const deviceItem2 = menuItems.find(i => i.getAttribute("label") === device2.name);
    ok(deviceItem1, "Test device 1 menu item added to device selector");
    ok(deviceItem2, "Test device 2 menu item added to device selector");
  });

  await openDeviceModal(ui);

  info("Remove device 2");
  const deviceRemoveButtons = [...document.querySelectorAll(".device-remove-button")];
  is(deviceRemoveButtons.length, 2, "Both devices have a remove button in modal");
  const removed = waitUntilState(store, state => state.devices.custom.length == 1);
  deviceRemoveButtons[1].click();
  await removed;
  document.getElementById("device-close-button").click();

  info("Ensure device 2 is no longer in device selector");
  await testMenuItems(toolWindow, deviceSelector, menuItems => {
    const deviceItem1 = menuItems.find(i => i.getAttribute("label") === device1.name);
    const deviceItem2 = menuItems.find(i => i.getAttribute("label") === device2.name);
    ok(deviceItem1, "Test device 1 menu item exists");
    ok(!deviceItem2, "Test device 2 menu item removed");
  });
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { store, document } = toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  const deviceSelector = document.getElementById("device-selector");

  info("Ensure device 1 is still in device selector");
  await testMenuItems(toolWindow, deviceSelector, menuItems => {
    const deviceItem1 = menuItems.find(i => i.getAttribute("label") === device1.name);
    const deviceItem2 = menuItems.find(i => i.getAttribute("label") === device2.name);
    ok(deviceItem1, "Test device 1 menu item exists");
    ok(!deviceItem2, "Test device 2 option removed");
  });

  await openDeviceModal(ui);

  info("Ensure device 1 is still in device modal");
  const deviceCbs =
    [...document.querySelectorAll(".device-type-custom .device-input-checkbox")];
  is(deviceCbs.length, 1, "Only 1 custom present in modal");
  const deviceCb1 = deviceCbs.find(cb => cb.value == device1.name);
  ok(deviceCb1 && deviceCb1.checked, "Test device 1 checkbox exists and enabled");

  info("Ensure device 2 is no longer in device modal");
  const deviceCb2 = deviceCbs.find(cb => cb.value == device2.name);
  ok(!deviceCb2, "Test device 2 checkbox does not exist");
});

