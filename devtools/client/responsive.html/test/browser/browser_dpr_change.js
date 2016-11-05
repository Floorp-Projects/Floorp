/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests changing viewport device
const TEST_URL = "data:text/html;charset=utf-8,DPR list test";
const DEFAULT_DPPX = window.devicePixelRatio;
const VIEWPORT_DPPX = DEFAULT_DPPX + 2;
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
  yield testResetWhenResizingViewport(ui);
  yield testChangingDPR(ui);
});

function* waitStartup(ui) {
  let { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  yield waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);
}

function* testDefaults(ui) {
  info("Test Defaults");

  yield testDevicePixelRatio(ui, window.devicePixelRatio);
  testViewportDPRSelect(ui, {value: window.devicePixelRatio, disabled: false});
  testViewportDeviceSelectLabel(ui, "no device selected");
}

function* testChangingDevice(ui) {
  info("Test Changing Device");

  let waitPixelRatioChange = onceDevicePixelRatioChange(ui);

  yield selectDevice(ui, testDevice.name);
  yield waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  yield waitPixelRatioChange;
  yield testDevicePixelRatio(ui, testDevice.pixelRatio);
  testViewportDPRSelect(ui, {value: testDevice.pixelRatio, disabled: true});
  testViewportDeviceSelectLabel(ui, testDevice.name);
}

function* testResetWhenResizingViewport(ui) {
  info("Test reset when resizing the viewport");

  let waitPixelRatioChange = onceDevicePixelRatioChange(ui);

  yield testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [testDevice.width, testDevice.height - 10], [0, -10], ui);

  yield waitPixelRatioChange;
  yield testDevicePixelRatio(ui, window.devicePixelRatio);

  testViewportDPRSelect(ui, {value: window.devicePixelRatio, disabled: false});
  testViewportDeviceSelectLabel(ui, "no device selected");
}

function* testChangingDPR(ui) {
  info("Test changing device pixel ratio");

  let waitPixelRatioChange = onceDevicePixelRatioChange(ui);

  yield selectDPR(ui, VIEWPORT_DPPX);
  yield waitPixelRatioChange;
  yield testDevicePixelRatio(ui, VIEWPORT_DPPX);
  testViewportDPRSelect(ui, {value: VIEWPORT_DPPX, disabled: false});
  testViewportDeviceSelectLabel(ui, "no device selected");
}

function testViewportDPRSelect(ui, expected) {
  info("Test viewport's DPR Select");

  let select = ui.toolWindow.document.querySelector("#global-dpr-selector > select");
  is(select.value, expected.value,
     `DPR Select value should be: ${expected.value}`);
  is(select.disabled, expected.disabled,
    `DPR Select should be ${expected.disabled ? "disabled" : "enabled"}.`);
}

function testViewportDeviceSelectLabel(ui, expected) {
  info("Test viewport's device select label");

  let select = ui.toolWindow.document.querySelector(".viewport-device-selector");
  is(select.selectedOptions[0].textContent, expected,
     `Device Select value should be: ${expected}`);
}

function* testDevicePixelRatio(ui, expected) {
  info("Test device pixel ratio");

  let dppx = yield getViewportDevicePixelRatio(ui);
  is(dppx, expected, `devicePixelRatio should be: ${expected}`);
}

function* getViewportDevicePixelRatio(ui) {
  return yield ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    return content.devicePixelRatio;
  });
}

function onceDevicePixelRatioChange(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, function* () {
    info(`Listening for a pixel ratio change (current: ${content.devicePixelRatio}dppx)`);

    let pixelRatio = content.devicePixelRatio;
    let mql = content.matchMedia(`(resolution: ${pixelRatio}dppx)`);

    return new Promise(resolve => {
      const onWindowCreated = () => {
        if (pixelRatio !== content.devicePixelRatio) {
          resolve();
        }
      };

      addEventListener("DOMWindowCreated", onWindowCreated, {once: true});

      mql.addListener(function listener() {
        mql.removeListener(listener);
        removeEventListener("DOMWindowCreated", onWindowCreated, {once: true});
        resolve();
      });
    });
  });
}
