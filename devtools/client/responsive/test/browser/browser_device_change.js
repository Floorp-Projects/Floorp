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

// Add the laptop to the device list
const {
  updatePreferredDevices,
} = require("devtools/client/responsive/actions/devices");
updatePreferredDevices({
  added: ["Laptop with MDPI screen"],
  removed: [],
});

addRDMTask(
  TEST_URL,
  async function({ ui }) {
    reloadOnUAChange(true);

    // Test defaults
    testViewportDimensions(ui, 320, 480);
    info("Should have default UA at the start of the test");
    await testUserAgent(ui, DEFAULT_UA);
    await testDevicePixelRatio(ui, DEFAULT_DPPX);
    await testTouchEventsOverride(ui, false);
    testViewportDeviceMenuLabel(ui, "Responsive");

    // Test device with custom properties
    await selectDevice(ui, "Fake Phone RDM Test");
    await waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
    info("Should have device UA now that device is applied");
    await testUserAgent(ui, testDevice.userAgent);
    await testDevicePixelRatio(ui, testDevice.pixelRatio);
    await testTouchEventsOverride(ui, true);

    // Test resetting device when resizing viewport
    await testViewportResize(
      ui,
      ".viewport-vertical-resize-handle",
      [-10, -10],
      [0, -10],
      {
        hasDevice: true,
      }
    );

    info("Should have default UA after resizing viewport");
    await testUserAgent(ui, DEFAULT_UA);
    await testDevicePixelRatio(ui, DEFAULT_DPPX);
    await testTouchEventsOverride(ui, false);
    testViewportDeviceMenuLabel(ui, "Responsive");

    // Test device with generic properties
    await selectDevice(ui, "Laptop with MDPI screen");
    await waitForViewportResizeTo(ui, 1280, 800);
    info("Should have default UA when using device without specific UA");
    await testUserAgent(ui, DEFAULT_UA);
    await testDevicePixelRatio(ui, 1);
    await testTouchEventsOverride(ui, false);

    reloadOnUAChange(false);
  },
  { waitForDeviceList: true }
);

addRDMTask(
  null,
  async function() {
    const tab = await addTab(TEST_URL);
    const { ui } = await openRDM(tab);

    const { store } = ui.toolWindow;

    reloadOnUAChange(true);

    // Wait until the viewport has been added and the device list has been loaded
    await waitUntilState(
      store,
      state =>
        state.viewports.length == 1 &&
        state.viewports[0].device === "Laptop with MDPI screen" &&
        state.devices.listState == Types.loadableState.LOADED
    );

    // Select device with custom UA
    const waitForReload = await watchForDevToolsReload(ui.getViewportBrowser());
    await selectDevice(ui, "Fake Phone RDM Test");
    await waitForReload();
    await waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
    info("Should have device UA now that device is applied");
    await testUserAgent(ui, testDevice.userAgent);

    // Browser will reload to clear the UA on RDM close
    const onReload = BrowserTestUtils.browserLoaded(ui.getViewportBrowser());
    await closeRDM(tab);
    await onReload;

    // Ensure UA is reset to default after closing RDM
    info("Should have default UA after closing RDM");
    await testUserAgentFromBrowser(tab.linkedBrowser, DEFAULT_UA);

    await removeTab(tab);

    reloadOnUAChange(false);
  },
  { onlyPrefAndTask: true }
);
