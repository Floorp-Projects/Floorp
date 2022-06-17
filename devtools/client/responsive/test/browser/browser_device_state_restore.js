/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the previous selected device is restored when reopening RDM.

const TEST_URL = "data:text/html;charset=utf-8,";
const DEFAULT_DPPX = window.devicePixelRatio;

/* eslint-disable max-len */
const TEST_DEVICE = {
  name: "iPhone 6/7/8",
  width: 375,
  height: 667,
  pixelRatio: 2,
  userAgent:
    "Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1",
  touch: true,
  firefoxOS: false,
  os: "iOS",
  featured: true,
};
/* eslint-enable max-len */

// Add the device to the list
const {
  updatePreferredDevices,
} = require("devtools/client/responsive/actions/devices");
updatePreferredDevices({
  added: [TEST_DEVICE.name],
  removed: [],
});

const Types = require("devtools/client/responsive/types");

addRDMTask(
  TEST_URL,
  async function({ ui }) {
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
    const waitForReload = await watchForDevToolsReload(ui.getViewportBrowser());
    await selectDevice(ui, TEST_DEVICE.name);
    await waitForReload();
    await waitForViewportResizeTo(ui, TEST_DEVICE.width, TEST_DEVICE.height);

    info("Checking the RDM device state.");
    testViewportDeviceMenuLabel(ui, TEST_DEVICE.name);
    await testUserAgent(ui, TEST_DEVICE.userAgent);
    await testDevicePixelRatio(ui, TEST_DEVICE.pixelRatio);
    await testTouchEventsOverride(ui, TEST_DEVICE.touch);

    reloadOnUAChange(false);
  },
  { waitForDeviceList: true }
);

addRDMTaskWithPreAndPost(
  TEST_URL,
  function rdmPreTask({ browser }) {
    reloadOnUAChange(true);
  },
  async function({ ui }) {
    // Note: This code might be racy. Call watchForDevToolsReload as early as
    // possible to catch the reload that will happen on RDM startup.
    // We cannot easily call watchForDevToolsReload in the preTask because it
    // needs RDM to be already started. Otherwise it will not find any devtools
    // UI to wait for.
    const waitForReload = await watchForDevToolsReload(ui.getViewportBrowser());

    const { store } = ui.toolWindow;

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
    await waitForReload();

    info("Checking the restored RDM state.");
    testViewportDeviceMenuLabel(ui, TEST_DEVICE.name);
    testViewportDimensions(ui, TEST_DEVICE.width, TEST_DEVICE.height);
    await testUserAgent(ui, TEST_DEVICE.userAgent);
    await testDevicePixelRatio(ui, TEST_DEVICE.pixelRatio);
    await testTouchEventsOverride(ui, TEST_DEVICE.touch);

    info("Rotating the viewport.");
    rotateViewport(ui);

    reloadOnUAChange(false);
  },
  function rdmPostTask({ browser }) {},
  { waitForDeviceList: true }
);

addRDMTask(
  TEST_URL,
  async function({ ui }) {
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
    const waitForReload = await watchForDevToolsReload(ui.getViewportBrowser());
    await waitForReload();

    info("Checking the restored RDM state.");
    testViewportDeviceMenuLabel(ui, TEST_DEVICE.name);
    testViewportDimensions(ui, TEST_DEVICE.height, TEST_DEVICE.width);
    await testUserAgent(ui, TEST_DEVICE.userAgent);
    await testDevicePixelRatio(ui, TEST_DEVICE.pixelRatio);
    await testTouchEventsOverride(ui, TEST_DEVICE.touch);

    reloadOnUAChange(false);
  },
  { waitForDeviceList: true }
);
