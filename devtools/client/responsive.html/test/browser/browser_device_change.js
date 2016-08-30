/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests changing viewport device
const TEST_URL = "data:text/html;charset=utf-8,Device list test";
const DEFAULT_UA = Cc["@mozilla.org/network/protocol;1?name=http"]
  .getService(Ci.nsIHttpProtocolHandler)
  .userAgent;
const NOKIA_UA = "Mozilla/5.0 (compatible; MSIE 10.0; Windows Phone 8.0; " +
  "Trident/6.0; IEMobile/10.0; ARM; Touch; NOKIA; Lumia 520)";
const Types = require("devtools/client/responsive.html/types");

addRDMTask(TEST_URL, function* ({ ui, manager }) {
  let { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);

  // Test defaults
  testViewportDimensions(ui, 320, 480);
  yield testUserAgent(ui, DEFAULT_UA);
  testViewportSelectLabel(ui, "no device selected");

  // Test device with custom UA
  switchDevice(ui, "Nokia Lumia 520");
  yield waitForViewportResizeTo(ui, 320, 533);
  yield testUserAgent(ui, NOKIA_UA);

  // Test resetting device when resizing viewport
  yield testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [320, 523], [0, -10], ui);
  yield testUserAgent(ui, DEFAULT_UA);
  testViewportSelectLabel(ui, "no device selected");

  // Test device where UA field is blank
  switchDevice(ui, "Laptop (1366 x 768)");
  yield waitForViewportResizeTo(ui, 1366, 768);
  yield testUserAgent(ui, DEFAULT_UA);
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
