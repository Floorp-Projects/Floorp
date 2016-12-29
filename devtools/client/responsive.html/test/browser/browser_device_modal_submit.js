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

addRDMTask(TEST_URL, function* ({ ui }) {
  let { store, document } = ui.toolWindow;
  let modal = document.querySelector("#device-modal-wrapper");
  let select = document.querySelector(".viewport-device-selector");
  let submitButton = document.querySelector("#device-submit-button");

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  openDeviceModal(ui);

  info("Checking displayed device checkboxes are checked in the device modal.");
  let checkedCbs = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => cb.checked);

  let remoteList = yield getDevices();

  let featuredCount = remoteList.TYPES.reduce((total, type) => {
    return total + remoteList[type].reduce((subtotal, device) => {
      return subtotal + ((device.os != "fxos" && device.featured) ? 1 : 0);
    }, 0);
  }, 0);

  is(featuredCount, checkedCbs.length,
    "Got expected number of displayed devices.");

  for (let cb of checkedCbs) {
    ok(Object.keys(remoteList).filter(type => remoteList[type][cb.value]),
      cb.value + " is correctly checked.");
  }

  // Tests where the user adds a non-featured device
  info("Check the first unchecked device and submit new device list.");
  let uncheckedCb = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => !cb.checked)[0];
  let value = uncheckedCb.value;
  uncheckedCb.click();
  submitButton.click();

  ok(modal.classList.contains("closed") && !modal.classList.contains("opened"),
    "The device modal is closed on submit.");

  info("Checking that the new device is added to the user preference list.");
  let preferredDevices = _loadPreferredDevices();
  ok(preferredDevices.added.has(value), value + " in user added list.");

  info("Checking new device is added to the device selector.");
  let options = [...select.options];
  is(options.length - 2, featuredCount + 1,
    "Got expected number of devices in device selector.");
  ok(options.filter(o => o.value === value)[0],
    value + " added to the device selector.");

  info("Reopen device modal and check new device is correctly checked");
  openDeviceModal(ui);
  ok([...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => cb.checked && cb.value === value)[0],
    value + " is checked in the device modal.");

  // Tests where the user removes a featured device
  info("Uncheck the first checked device different than the previous one");
  let checkedCb = [...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => cb.checked && cb.value != value)[0];
  let checkedVal = checkedCb.value;
  checkedCb.click();
  submitButton.click();

  info("Checking that the device is removed from the user preference list.");
  preferredDevices = _loadPreferredDevices();
  ok(preferredDevices.removed.has(checkedVal), checkedVal + " in removed list");

  info("Checking that the device is not in the device selector.");
  options = [...select.options];
  is(options.length - 2, featuredCount,
    "Got expected number of devices in device selector.");
  ok(!options.filter(o => o.value === checkedVal)[0],
    checkedVal + " removed from the device selector.");

  info("Reopen device modal and check device is correctly unchecked");
  openDeviceModal(ui);
  ok([...document.querySelectorAll(".device-input-checkbox")]
    .filter(cb => !cb.checked && cb.value === checkedVal)[0],
    checkedVal + " is unchecked in the device modal.");

  // Let's add a dummy device to simulate featured flag changes for next test
  addDeviceForTest(addedDevice);
});

addRDMTask(TEST_URL, function* ({ ui }) {
  let { store, document } = ui.toolWindow;
  let select = document.querySelector(".viewport-device-selector");

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  openDeviceModal(ui);

  let remoteList = yield getDevices();
  let featuredCount = remoteList.TYPES.reduce((total, type) => {
    return total + remoteList[type].reduce((subtotal, device) => {
      return subtotal + ((device.os != "fxos" && device.featured) ? 1 : 0);
    }, 0);
  }, 0);
  let preferredDevices = _loadPreferredDevices();

  // Tests to prove that reloading the RDM didn't break our device list
  info("Checking new featured device appears in the device selector.");
  let options = [...select.options];
  is(options.length - 2, featuredCount
    - preferredDevices.removed.size + preferredDevices.added.size,
    "Got expected number of devices in device selector.");

  ok(options.filter(o => o.value === addedDevice.name)[0],
    "dummy device added to the device selector.");

  ok(options.filter(o => preferredDevices.added.has(o.value))[0],
    "device added by user still in the device selector.");

  ok(!options.filter(o => preferredDevices.removed.has(o.value))[0],
    "device removed by user not in the device selector.");
});
