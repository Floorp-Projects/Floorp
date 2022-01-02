/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests changing viewport touch simulation
const TEST_URL = "data:text/html;charset=utf-8,touch simulation test";
const Types = require("devtools/client/responsive/types");

const testDevice = {
  name: "Fake Phone RDM Test",
  width: 320,
  height: 470,
  pixelRatio: 5.5,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: true,
  firefoxOS: true,
  os: "custom",
  featured: true,
};

// Add the new device to the list
addDeviceForTest(testDevice);

addRDMTask(
  TEST_URL,
  async function({ ui }) {
    reloadOnTouchChange(true);

    await waitStartup(ui);

    await testDefaults(ui);
    await testChangingDevice(ui);
    await testResizingViewport(ui, true, false);
    await testEnableTouchSimulation(ui);
    await testResizingViewport(ui, false, true);
    await testDisableTouchSimulation(ui);

    reloadOnTouchChange(false);
  },
  { waitForDeviceList: true }
);

async function waitStartup(ui) {
  const { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(
    store,
    state =>
      state.viewports.length == 1 &&
      state.devices.listState == Types.loadableState.LOADED
  );
}

async function testDefaults(ui) {
  info("Test Defaults");

  await testTouchEventsOverride(ui, false);
  testViewportDeviceMenuLabel(ui, "Responsive");
}

async function testChangingDevice(ui) {
  info("Test Changing Device");

  await selectDevice(ui, testDevice.name);
  await waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  await testTouchEventsOverride(ui, true);
  testViewportDeviceMenuLabel(ui, testDevice.name);
}

async function testResizingViewport(ui, hasDevice, touch) {
  info(`Test resizing the viewport, device ${hasDevice}, touch ${touch}`);

  await testViewportResize(
    ui,
    ".viewport-vertical-resize-handle",
    [-10, -10],
    [0, -10],
    {
      hasDevice,
    }
  );
  await testTouchEventsOverride(ui, touch);
  testViewportDeviceMenuLabel(ui, "Responsive");
}

async function testEnableTouchSimulation(ui) {
  info("Test enabling touch simulation via button");

  await toggleTouchSimulation(ui);
  await testTouchEventsOverride(ui, true);
}

async function testDisableTouchSimulation(ui) {
  info("Test disabling touch simulation via button");

  await toggleTouchSimulation(ui);
  await testTouchEventsOverride(ui, false);
}
