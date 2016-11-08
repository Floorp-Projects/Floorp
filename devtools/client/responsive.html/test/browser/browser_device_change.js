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
addDeviceForTest(testDevice);

addRDMTask(TEST_URL, function* ({ ui, manager }) {
  let { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  // Test defaults
  testViewportDimensions(ui, 320, 480);
  yield testUserAgent(ui, DEFAULT_UA);
  yield testDevicePixelRatio(ui, DEFAULT_DPPX);
  yield testTouchEventsOverride(ui, false);
  testViewportSelectLabel(ui, "no device selected");

  // Test device with custom properties
  yield selectDevice(ui, "Fake Phone RDM Test");
  yield waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  yield testUserAgent(ui, testDevice.userAgent);
  yield testDevicePixelRatio(ui, testDevice.pixelRatio);
  yield testTouchEventsOverride(ui, true);

  // Test resetting device when resizing viewport
  let deviceChanged = once(ui, "viewport-device-changed");
  yield testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [testDevice.width, testDevice.height - 10], [0, -10], ui);
  yield deviceChanged;
  yield testUserAgent(ui, DEFAULT_UA);
  yield testDevicePixelRatio(ui, DEFAULT_DPPX);
  yield testTouchEventsOverride(ui, false);
  testViewportSelectLabel(ui, "no device selected");

  // Test device with generic properties
  yield selectDevice(ui, "Laptop (1366 x 768)");
  yield waitForViewportResizeTo(ui, 1366, 768);
  yield testUserAgent(ui, DEFAULT_UA);
  yield testDevicePixelRatio(ui, 1);
  yield testTouchEventsOverride(ui, false);
});

function testViewportDimensions(ui, w, h) {
  let viewport = ui.toolWindow.document.querySelector(".viewport-content");

  is(ui.toolWindow.getComputedStyle(viewport).getPropertyValue("width"),
     `${w}px`, `Viewport should have width of ${w}px`);
  is(ui.toolWindow.getComputedStyle(viewport).getPropertyValue("height"),
     `${h}px`, `Viewport should have height of ${h}px`);
}

function testViewportSelectLabel(ui, expected) {
  let select = ui.toolWindow.document.querySelector(".viewport-device-selector");
  is(select.selectedOptions[0].textContent, expected,
     `Select label should be changed to ${expected}`);
}

function* testUserAgent(ui, expected) {
  let ua = yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    return content.navigator.userAgent;
  });
  is(ua, expected, `UA should be set to ${expected}`);
}

function* testDevicePixelRatio(ui, expected) {
  let dppx = yield getViewportDevicePixelRatio(ui);
  is(dppx, expected, `devicePixelRatio should be set to ${expected}`);
}

function* testTouchEventsOverride(ui, expected) {
  let { document } = ui.toolWindow;
  let touchButton = document.querySelector("#global-touch-simulation-button");

  let flag = yield ui.emulationFront.getTouchEventsOverride();
  is(flag === Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_ENABLED, expected,
    `Touch events override should be ${expected ? "enabled" : "disabled"}`);
  is(touchButton.classList.contains("active"), expected,
    `Touch simulation button should be ${expected ? "" : "not"} active.`);
}

function* getViewportDevicePixelRatio(ui) {
  return yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    return content.devicePixelRatio;
  });
}
