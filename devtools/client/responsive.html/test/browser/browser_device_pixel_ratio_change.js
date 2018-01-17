/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests changing viewport device pixel ratio
const TEST_URL = "data:text/html;charset=utf-8,DevicePixelRatio list test";
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

addRDMTask(TEST_URL, async function ({ ui, manager }) {
  await waitStartup(ui);

  await testDefaults(ui);
  await testChangingDevice(ui);
  await testResetWhenResizingViewport(ui);
  await testChangingDevicePixelRatio(ui);
});

async function waitStartup(ui) {
  let { store } = ui.toolWindow;

  // Wait until the viewport has been added and the device list has been loaded
  await waitUntilState(store, state => state.viewports.length == 1
    && state.devices.listState == Types.deviceListState.LOADED);
}

async function testDefaults(ui) {
  info("Test Defaults");

  await testDevicePixelRatio(ui, window.devicePixelRatio);
  testViewportDevicePixelRatioSelect(ui, {
    value: window.devicePixelRatio,
    disabled: false,
  });
  testViewportDeviceSelectLabel(ui, "no device selected");
}

async function testChangingDevice(ui) {
  info("Test Changing Device");

  let waitPixelRatioChange = onceDevicePixelRatioChange(ui);

  await selectDevice(ui, testDevice.name);
  await waitForViewportResizeTo(ui, testDevice.width, testDevice.height);
  await waitPixelRatioChange;
  await testDevicePixelRatio(ui, testDevice.pixelRatio);
  testViewportDevicePixelRatioSelect(ui, {
    value: testDevice.pixelRatio,
    disabled: true,
  });
  testViewportDeviceSelectLabel(ui, testDevice.name);
}

async function testResetWhenResizingViewport(ui) {
  info("Test reset when resizing the viewport");

  let waitPixelRatioChange = onceDevicePixelRatioChange(ui);

  let deviceRemoved = once(ui, "device-association-removed");
  await testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [testDevice.width, testDevice.height - 10], [0, -10], ui);
  await deviceRemoved;

  await waitPixelRatioChange;
  await testDevicePixelRatio(ui, window.devicePixelRatio);

  testViewportDevicePixelRatioSelect(ui, {
    value: window.devicePixelRatio,
    disabled: false,
  });
  testViewportDeviceSelectLabel(ui, "no device selected");
}

async function testChangingDevicePixelRatio(ui) {
  info("Test changing device pixel ratio");

  let waitPixelRatioChange = onceDevicePixelRatioChange(ui);

  await selectDevicePixelRatio(ui, VIEWPORT_DPPX);
  await waitPixelRatioChange;
  await testDevicePixelRatio(ui, VIEWPORT_DPPX);
  testViewportDevicePixelRatioSelect(ui, {
    value: VIEWPORT_DPPX,
    disabled: false,
  });
  testViewportDeviceSelectLabel(ui, "no device selected");
}

function testViewportDevicePixelRatioSelect(ui, expected) {
  info("Test viewport's DevicePixelRatio Select");

  let select =
    ui.toolWindow.document.querySelector("#global-device-pixel-ratio-selector");
  is(select.value, expected.value,
     `DevicePixelRatio Select value should be: ${expected.value}`);
  is(select.disabled, expected.disabled,
    `DevicePixelRatio Select should be ${expected.disabled ? "disabled" : "enabled"}.`);
}

async function testDevicePixelRatio(ui, expected) {
  info("Test device pixel ratio");

  let dppx = await getViewportDevicePixelRatio(ui);
  is(dppx, expected, `devicePixelRatio should be: ${expected}`);
}

function onceDevicePixelRatioChange(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, async function () {
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
