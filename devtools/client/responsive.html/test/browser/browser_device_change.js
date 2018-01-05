/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests changing viewport device (need HTTP load for proper UA testing)
const TEST_URL = `${URL_ROOT}doc_page_state.html`;

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

addRDMTask(TEST_URL, function* ({ ui }) {
  let { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  // Test defaults
  testViewportDimensions(ui, 320, 480);
  info("Should have default UA at the start of the test");
  yield testUserAgent(ui, DEFAULT_UA);
  yield testDevicePixelRatio(ui, DEFAULT_DPPX);
  yield testTouchEventsOverride(ui, false);
  testViewportDeviceSelectLabel(ui, "no device selected");

  // Test device with custom properties
  let reloaded = waitForViewportLoad(ui);
  yield selectDevice(ui, "Fake Phone RDM Test");
  yield reloaded;
  yield waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  info("Should have device UA now that device is applied");
  yield testUserAgent(ui, testDevice.userAgent);
  yield testDevicePixelRatio(ui, testDevice.pixelRatio);
  yield testTouchEventsOverride(ui, true);

  // Test resetting device when resizing viewport
  let deviceRemoved = once(ui, "device-association-removed");
  reloaded = waitForViewportLoad(ui);
  yield testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [testDevice.width, testDevice.height - 10], [0, -10], ui);
  yield Promise.all([ deviceRemoved, reloaded ]);
  info("Should have default UA after resizing viewport");
  yield testUserAgent(ui, DEFAULT_UA);
  yield testDevicePixelRatio(ui, DEFAULT_DPPX);
  yield testTouchEventsOverride(ui, false);
  testViewportDeviceSelectLabel(ui, "no device selected");

  // Test device with generic properties
  yield selectDevice(ui, "Laptop (1366 x 768)");
  yield waitForViewportResizeTo(ui, 1366, 768);
  info("Should have default UA when using device without specific UA");
  yield testUserAgent(ui, DEFAULT_UA);
  yield testDevicePixelRatio(ui, 1);
  yield testTouchEventsOverride(ui, false);
});

add_task(async function () {
  const tab = await addTab(TEST_URL);
  const { ui } = await openRDM(tab);

  let { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

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
});

function testViewportDimensions(ui, w, h) {
  let viewport = ui.toolWindow.document.querySelector(".viewport-content");

  is(ui.toolWindow.getComputedStyle(viewport).getPropertyValue("width"),
     `${w}px`, `Viewport should have width of ${w}px`);
  is(ui.toolWindow.getComputedStyle(viewport).getPropertyValue("height"),
     `${h}px`, `Viewport should have height of ${h}px`);
}

function* testDevicePixelRatio(ui, expected) {
  let dppx = yield getViewportDevicePixelRatio(ui);
  is(dppx, expected, `devicePixelRatio should be set to ${expected}`);
}

function* getViewportDevicePixelRatio(ui) {
  return yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    return content.devicePixelRatio;
  });
}
