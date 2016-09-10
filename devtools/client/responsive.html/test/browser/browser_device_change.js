/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests changing viewport device
const TEST_URL = "data:text/html;charset=utf-8,Device list test";

const DEFAULT_DPPX = window.devicePixelRatio;
const DEFAULT_UA = Cc["@mozilla.org/network/protocol;1?name=http"]
  .getService(Ci.nsIHttpProtocolHandler)
  .userAgent;

const Types = require("devtools/client/responsive.html/types");

const { addDevice, removeDevice } = require("devtools/client/shared/devices");

const testDevice = {
  "name": "Fake Phone RDM Test",
  "width": 320,
  "height": 570,
  "pixelRatio": 5.5,
  "userAgent": "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  "touch": true,
  "firefoxOS": true,
  "os": "custom",
  "featured": true,
};

// Add the new device to the list
addDevice(testDevice);

addRDMTask(TEST_URL, function* ({ ui, manager }) {
  let { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  // Test defaults
  testViewportDimensions(ui, 320, 480);
  yield testUserAgent(ui, DEFAULT_UA);
  testDevicePixelRatio(yield getViewportDevicePixelRatio(ui), DEFAULT_DPPX);
  testViewportSelectLabel(ui, "no device selected");

  let waitingPixelRatio = onceDevicePixelRatioChange(ui);

  // Test device with custom UA
  yield switchDevice(ui, "Fake Phone RDM Test");
  yield waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  yield testUserAgent(ui, testDevice.userAgent);

  // Test device with custom pixelRatio
  testDevicePixelRatio(yield waitingPixelRatio, testDevice.pixelRatio);
  waitingPixelRatio = onceDevicePixelRatioChange(ui);

  // Test resetting device when resizing viewport
  yield testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [testDevice.width, testDevice.height - 10], [0, -10], ui);
  yield testUserAgent(ui, DEFAULT_UA);
  testViewportSelectLabel(ui, "no device selected");
  testDevicePixelRatio(yield waitingPixelRatio, DEFAULT_DPPX);

  // Test device where UA field is blank
  yield switchDevice(ui, "Laptop (1366 x 768)");
  yield waitForViewportResizeTo(ui, 1366, 768);
  yield testUserAgent(ui, DEFAULT_UA);

  ok(removeDevice(testDevice),
    "Test Device properly removed.");
});

function testViewportDimensions(ui, w, h) {
  let viewport = ui.toolWindow.document.querySelector(".viewport-content");

  is(ui.toolWindow.getComputedStyle(viewport).getPropertyValue("width"),
     `${w}px`, `Viewport should have width of ${w}px`);
  is(ui.toolWindow.getComputedStyle(viewport).getPropertyValue("height"),
     `${h}px`, `Viewport should have height of ${h}px`);
}

function testViewportSelectLabel(ui, label) {
  let select = ui.toolWindow.document.querySelector(".viewport-device-selector");
  is(select.selectedOptions[0].textContent, label,
     `Select label should be changed to ${label}`);
}

function* testUserAgent(ui, value) {
  let ua = yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    return content.navigator.userAgent;
  });
  is(ua, value, `UA should be set to ${value}`);
}

function testDevicePixelRatio(dppx, expected) {
  is(dppx, expected, `devicePixelRatio should be set to ${expected}`);
}

function* getViewportDevicePixelRatio(ui) {
  return yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    return content.devicePixelRatio;
  });
}

function onceDevicePixelRatioChange(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    let pixelRatio = content.devicePixelRatio;
    let mql = content.matchMedia(`(resolution: ${pixelRatio}dppx)`);

    return new Promise(resolve => {
      mql.addListener(function listener() {
        mql.removeListener(listener);
        resolve(content.devicePixelRatio);
      });
    });
  });
}
