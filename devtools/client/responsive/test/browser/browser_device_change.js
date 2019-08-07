/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests changing viewport device (need HTTP load for proper UA testing)

const TEST_URL = `${URL_ROOT}doc_page_state.html`;
const DEFAULT_DPPX = window.devicePixelRatio;

const Types = require("devtools/client/responsive/types");

const testDevice = {
  name: "Fake Phone RDM Test",
  width: 320,
  height: 570,
  pixelRatio: 5.5,
  userAgent: "Mozilla/5.0 (Mobile; rv:39.0) Gecko/39.0 Firefox/39.0",
  touch: true,
  firefoxOS: true,
  os: "custom",
  featured: true,
};

// Add the new device to the list
addDeviceForTest(testDevice);

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

  // Test defaults
  testViewportDimensions(ui, 320, 480);
  info("Should have default UA at the start of the test");
  await testUserAgent(ui, DEFAULT_UA);
  await testDevicePixelRatio(ui, DEFAULT_DPPX);
  await testTouchEventsOverride(ui, false);
  testViewportDeviceMenuLabel(ui, "Responsive");

  // Test device with custom properties
  let reloaded = waitForViewportLoad(ui);
  await selectDevice(ui, "Fake Phone RDM Test");
  await reloaded;
  await waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  info("Should have device UA now that device is applied");
  await testUserAgent(ui, testDevice.userAgent);
  await testDevicePixelRatio(ui, testDevice.pixelRatio);
  await testTouchEventsOverride(ui, true);

  // Test resetting device when resizing viewport
  const deviceRemoved = once(ui, "device-association-removed");
  reloaded = waitForViewportLoad(ui);
  await testViewportResize(
    ui,
    ".viewport-vertical-resize-handle",
    [-10, -10],
    [testDevice.width, testDevice.height - 10],
    [0, -10],
    ui
  );
  await Promise.all([deviceRemoved, reloaded]);
  info("Should have default UA after resizing viewport");
  await testUserAgent(ui, DEFAULT_UA);
  await testDevicePixelRatio(ui, DEFAULT_DPPX);
  await testTouchEventsOverride(ui, false);
  testViewportDeviceMenuLabel(ui, "Responsive");

  // Test device with generic properties
  await selectDevice(ui, "Laptop (1366 x 768)");
  await waitForViewportResizeTo(ui, 1366, 768);
  info("Should have default UA when using device without specific UA");
  await testUserAgent(ui, DEFAULT_UA);
  await testDevicePixelRatio(ui, 1);
  await testTouchEventsOverride(ui, false);

  reloadOnUAChange(false);
});

add_task(async function() {
  const tab = await addTab(TEST_URL);
  const { ui } = await openRDM(tab);

  const { store } = ui.toolWindow;

  reloadOnUAChange(true);

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(
    store,
    state =>
      state.viewports.length == 1 &&
      state.viewports[0].device === "Laptop (1366 x 768)" &&
      state.devices.listState == Types.loadableState.LOADED
  );

  // Select device with custom UA
  let reloaded = waitForViewportLoad(ui);
  await selectDevice(ui, "Fake Phone RDM Test");
  await reloaded;
  await waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  info("Should have device UA now that device is applied");
  await testUserAgent(ui, testDevice.userAgent);

  // Browser will reload to clear the UA on RDM close
  reloaded = waitForViewportLoad(ui);
  await closeRDM(tab);
  await reloaded;

  // Ensure UA is reset to default after closing RDM
  info("Should have default UA after closing RDM");
  await testUserAgentFromBrowser(tab.linkedBrowser, DEFAULT_UA);

  await removeTab(tab);

  reloadOnUAChange(false);
});
