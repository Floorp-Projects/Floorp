/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the previous selected device is restored when reopening RDM.

const TEST_URL = "data:text/html;charset=utf-8,";
const DEFAULT_DPPX = window.devicePixelRatio;

/* eslint-disable max-len */
const TEST_DEVICE = {
  name: "Apple iPhone 6s",
  width: 375,
  height: 667,
  pixelRatio: 2,
  userAgent:
    "Mozilla/5.0 (iPhone; CPU iPhone OS 8_0 like Mac OS X) AppleWebKit/600.1.3 (KHTML, like Gecko) Version/8.0 Mobile/12A4345d Safari/600.1.4",
  touch: true,
  firefoxOS: false,
  os: "ios",
  featured: true,
};
/* eslint-enable max-len */

const Types = require("devtools/client/responsive/types");

addRDMTask(TEST_URL, async function({ ui }) {
  const { store } = ui.toolWindow;

  reloadOnUAChange(true);

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(
    store,
    state =>
      state.viewports.length == 1 &&
      state.devices.listState == Types.loadableState.LOADED
  );

  info("Checking the default RDM state.");
  testViewportDeviceMenuLabel(ui, "Responsive");
  testViewportDimensions(ui, 320, 480);
  await testUserAgent(ui, DEFAULT_UA);
  await testDevicePixelRatio(ui, DEFAULT_DPPX);
  await testTouchEventsOverride(ui, false);

  info("Select a device");
  const reloaded = waitForViewportLoad(ui);
  await selectDevice(ui, TEST_DEVICE.name);
  await reloaded;
  await waitForViewportResizeTo(ui, TEST_DEVICE.width, TEST_DEVICE.height);

  info("Checking the RDM device state.");
  testViewportDeviceMenuLabel(ui, TEST_DEVICE.name);
  await testUserAgent(ui, TEST_DEVICE.userAgent);
  await testDevicePixelRatio(ui, TEST_DEVICE.pixelRatio);
  await testTouchEventsOverride(ui, TEST_DEVICE.touch);

  reloadOnUAChange(false);
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { store } = ui.toolWindow;

  reloadOnUAChange(true);

  info(
    "Reopening RDM and checking that the previous device state is restored."
  );

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(
    store,
    state =>
      state.viewports.length == 1 &&
      state.viewports[0].device === TEST_DEVICE.name &&
      state.devices.listState == Types.loadableState.LOADED
  );
  await waitForViewportResizeTo(ui, TEST_DEVICE.width, TEST_DEVICE.height);
  await waitForViewportLoad(ui);

  info("Checking the restored RDM state.");
  testViewportDeviceMenuLabel(ui, TEST_DEVICE.name);
  testViewportDimensions(ui, TEST_DEVICE.width, TEST_DEVICE.height);
  await testUserAgent(ui, TEST_DEVICE.userAgent);
  await testDevicePixelRatio(ui, TEST_DEVICE.pixelRatio);
  await testTouchEventsOverride(ui, TEST_DEVICE.touch);

  info("Rotating the viewport.");
  rotateViewport(ui);

  reloadOnUAChange(false);
});

addRDMTask(TEST_URL, async function({ ui }) {
  const { store } = ui.toolWindow;

  reloadOnUAChange(true);

  info(
    "Reopening RDM and checking that the previous device state is restored."
  );

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(
    store,
    state =>
      state.viewports.length == 1 &&
      state.viewports[0].device === TEST_DEVICE.name &&
      state.devices.listState == Types.loadableState.LOADED
  );
  await waitForViewportResizeTo(ui, TEST_DEVICE.height, TEST_DEVICE.width);
  await waitForViewportLoad(ui);

  info("Checking the restored RDM state.");
  testViewportDeviceMenuLabel(ui, TEST_DEVICE.name);
  testViewportDimensions(ui, TEST_DEVICE.height, TEST_DEVICE.width);
  await testUserAgent(ui, TEST_DEVICE.userAgent);
  await testDevicePixelRatio(ui, TEST_DEVICE.pixelRatio);
  await testTouchEventsOverride(ui, TEST_DEVICE.touch);

  reloadOnUAChange(false);
});
