/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test submitting display device changes on the device modal
const { getDevices } = require("devtools/client/shared/devices");

const addedDevice = {
  "name": "Fake Phone RDM Test",
  "width": 320,
  "height": 570,
  "pixelRatio": 1.5,
  "userAgent": "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  "touch": true,
  "firefoxOS": false,
  "os": "custom",
  "featured": true,
};

const TEST_URL = "data:text/html;charset=utf-8,";
const Types = require("devtools/client/responsive.html/types");

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { document, store } = toolWindow;
  const deviceSelector = document.getElementById("device-selector");

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  await openDeviceModal(ui);

  info("Checking displayed device checkboxes are checked in the device modal.");
  const checkedCbs = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => cb.checked);

  const remoteList = await getDevices();

  const featuredCount = remoteList.TYPES.reduce((total, type) => {
    return total + remoteList[type].reduce((subtotal, device) => {
      return subtotal + ((device.os != "fxos" && device.featured) ? 1 : 0);
    }, 0);
  }, 0);

  is(featuredCount, checkedCbs.length,
    "Got expected number of displayed devices.");

  for (const cb of checkedCbs) {
    ok(Object.keys(remoteList).filter(type => remoteList[type][cb.value]),
      cb.value + " is correctly checked.");
  }

  // Tests where the user adds a non-featured device
  info("Check the first unchecked device and submit new device list.");
  const uncheckedCb = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => !cb.checked)[0];
  const value = uncheckedCb.value;
  uncheckedCb.click();
  document.getElementById("device-close-button").click();

  ok(!store.getState().devices.isModalOpen, "The device modal is closed on submit.");

  info("Checking that the new device is added to the user preference list.");
  let preferredDevices = _loadPreferredDevices();
  ok(preferredDevices.added.has(value), value + " in user added list.");

  info("Checking new device is added to the device selector.");
  await testMenuItems(toolWindow, deviceSelector, menuItems => {
    is(menuItems.length - 2, featuredCount + 1,
      "Got expected number of devices in device selector.");

    const menuItem = menuItems.find(item => item.getAttribute("label") === name);
    ok(menuItem, value + " added to the device selector.");
  });

  info("Reopen device modal and check new device is correctly checked");
  await openDeviceModal(ui);

  ok([...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => cb.checked && cb.value === value)[0],
    value + " is checked in the device modal.");

  // Tests where the user removes a featured device
  info("Uncheck the first checked device different than the previous one");
  const checkedCb = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => cb.checked && cb.value != value)[0];
  const checkedVal = checkedCb.value;
  checkedCb.click();
  document.getElementById("device-close-button").click();

  info("Checking that the device is removed from the user preference list.");
  preferredDevices = _loadPreferredDevices();
  ok(preferredDevices.removed.has(checkedVal), checkedVal + " in removed list");

  info("Checking that the device is not in the device selector.");
  await testMenuItems(toolWindow, deviceSelector, menuItems => {
    is(menuItems.length - 2, featuredCount,
      "Got expected number of devices in device selector.");

    const menuItem = menuItems.find(item => item.getAttribute("label") === checkedVal);
    ok(!menuItem, checkedVal + " removed from the device selector.");
  });

  info("Reopen device modal and check device is correctly unchecked");
  await openDeviceModal(ui);
  ok([...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => !cb.checked && cb.value === checkedVal)[0],
    checkedVal + " is unchecked in the device modal.");

  // Let's add a dummy device to simulate featured flag changes for next test
  addDeviceForTest(addedDevice);
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { document, store } = toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.loadableState.LOADED);

  await openDeviceModal(ui);

  const remoteList = await getDevices();
  const featuredCount = remoteList.TYPES.reduce((total, type) => {
    return total + remoteList[type].reduce((subtotal, device) => {
      return subtotal + ((device.os != "fxos" && device.featured) ? 1 : 0);
    }, 0);
  }, 0);
  const preferredDevices = _loadPreferredDevices();

  // Tests to prove that reloading the RDM didn't break our device list
  info("Checking new featured device appears in the device selector.");
  const deviceSelector = document.getElementById("device-selector");
  await testMenuItems(toolWindow, deviceSelector, items => {
    is(items.length - 2, featuredCount
      - preferredDevices.removed.size + preferredDevices.added.size,
      "Got expected number of devices in device selector.");

    const added = items.find(i => i.getAttribute("label") === addedDevice.name);
    ok(added, "Dummy device added to the device selector.");

    for (const name of preferredDevices.added.keys()) {
      const menuItem = items.find(item => item.getAttribute("label") === name);
      ok(menuItem, "Device added by user still in the device selector.");
    }

    for (const name of preferredDevices.removed.keys()) {
      const menuItem = items.find(item => item.getAttribute("label") === name);
      ok(!menuItem, "Device removed by user not in the device selector.");
    }
  });
});
