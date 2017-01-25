/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests changing viewport touch simulation
const TEST_URL = "data:text/html;charset=utf-8,touch simulation test";
const Types = require("devtools/client/responsive.html/types");

const testDevice = {
  "name": "Fake Phone RDM Test",
  "width": 320,
  "height": 470,
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
  yield waitStartup(ui);

  yield testDefaults(ui);
  yield testChangingDevice(ui);
  yield testResizingViewport(ui, true, false);
  yield testEnableTouchSimulation(ui);
  yield testResizingViewport(ui, false, true);
});

function* waitStartup(ui) {
  let { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);
}

function* testDefaults(ui) {
  info("Test Defaults");

  yield testTouchEventsOverride(ui, false);
  testViewportDeviceSelectLabel(ui, "no device selected");
}

function* testChangingDevice(ui) {
  info("Test Changing Device");

  yield selectDevice(ui, testDevice.name);
  yield waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  yield testTouchEventsOverride(ui, true);
  testViewportDeviceSelectLabel(ui, testDevice.name);
}

function* testResizingViewport(ui, device, expected) {
  info(`Test resizing the viewport, device ${device}, expected ${expected}`);

  let deviceRemoved = once(ui, "device-removed");
  yield testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [testDevice.width, testDevice.height - 10], [0, -10], ui);
  if (device) {
    yield deviceRemoved;
  }
  yield testTouchEventsOverride(ui, expected);
  testViewportDeviceSelectLabel(ui, "no device selected");
}

function* testEnableTouchSimulation(ui) {
  info("Test enabling touch simulation via button");

  yield enableTouchSimulation(ui);
  yield testTouchEventsOverride(ui, true);
}
