/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from ../../../shared/test/shared-redux-head.js */
/* import-globals-from ../../../commandline/test/helpers.js */
/* import-globals-from ../../../inspector/test/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this);

// Import the GCLI test helper
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/commandline/test/helpers.js",
  this);

// Import helpers registering the test-actor in remote targets
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/test-actor-registry.js",
  this);

// Import helpers for the inspector that are also shared with others
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this);

const E10S_MULTI_ENABLED = Services.prefs.getIntPref("dom.ipc.processCount") > 1;
const TEST_URI_ROOT = "http://example.com/browser/devtools/client/responsive.html/test/browser/";
const OPEN_DEVICE_MODAL_VALUE = "OPEN_DEVICE_MODAL";
const RELOAD_CONDITION_PREF_PREFIX = "devtools.responsive.reloadConditions.";

const { _loadPreferredDevices } = require("devtools/client/responsive.html/actions/devices");
const asyncStorage = require("devtools/shared/async-storage");
const { addDevice, removeDevice, removeLocalDevices } = require("devtools/client/shared/devices");

SimpleTest.requestCompleteLog();
SimpleTest.waitForExplicitFinish();

// Toggling the RDM UI involves several docShell swap operations, which are somewhat slow
// on debug builds. Usually we are just barely over the limit, so a blanket factor of 2
// should be enough.
requestLongerTimeout(2);

Services.prefs.setCharPref("devtools.devices.url", TEST_URI_ROOT + "devices.json");
// The appearance of this notification causes intermittent behavior in some tests that
// send mouse events, since it causes the content to shift when it appears.
Services.prefs.setBoolPref("devtools.responsive.reloadNotification.enabled", false);

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("devtools.devices.url");
  Services.prefs.clearUserPref("devtools.responsive.reloadNotification.enabled");
  Services.prefs.clearUserPref("devtools.responsive.html.displayedDeviceList");
  Services.prefs.clearUserPref("devtools.responsive.reloadConditions.touchSimulation");
  Services.prefs.clearUserPref("devtools.responsive.reloadConditions.userAgent");
  await asyncStorage.removeItem("devtools.devices.url_cache");
  await removeLocalDevices();
});

loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsive.html/manager", true);

/**
 * Open responsive design mode for the given tab.
 */
var openRDM = async function(tab) {
  info("Opening responsive design mode");
  const manager = ResponsiveUIManager;
  const ui = await manager.openIfNeeded(tab.ownerGlobal, tab, { trigger: "test" });
  info("Responsive design mode opened");
  return { ui, manager };
};

/**
 * Close responsive design mode for the given tab.
 */
var closeRDM = async function(tab, options) {
  info("Closing responsive design mode");
  const manager = ResponsiveUIManager;
  await manager.closeIfNeeded(tab.ownerGlobal, tab, options);
  info("Responsive design mode closed");
};

/**
 * Adds a new test task that adds a tab with the given URL, opens responsive
 * design mode, runs the given generator, closes responsive design mode, and
 * removes the tab.
 *
 * Example usage:
 *
 *   addRDMTask(TEST_URL, async function ({ ui, manager }) {
 *     // Your tests go here...
 *   });
 */
function addRDMTask(url, task) {
  add_task(async function() {
    const tab = await addTab(url);
    const results = await openRDM(tab);

    try {
      await task(results);
    } catch (err) {
      ok(false, "Got an error: " + DevToolsUtils.safeErrorString(err));
    }

    await closeRDM(tab);
    await removeTab(tab);
  });
}

function spawnViewportTask(ui, args, task) {
  return ContentTask.spawn(ui.getViewportBrowser(), args, task);
}

function waitForFrameLoad(ui, targetURL) {
  return spawnViewportTask(ui, { targetURL }, async function(args) {
    if ((content.document.readyState == "complete" ||
         content.document.readyState == "interactive") &&
        content.location.href == args.targetURL) {
      return;
    }
    await ContentTaskUtils.waitForEvent(this, "DOMContentLoaded");
  });
}

function waitForViewportResizeTo(ui, width, height) {
  return new Promise(async function(resolve) {
    const isSizeMatching = data => data.width == width && data.height == height;

    // If the viewport has already the expected size, we resolve the promise immediately.
    const size = await getContentSize(ui);
    if (isSizeMatching(size)) {
      info(`Content already resized to ${width} x ${height}`);
      resolve();
      return;
    }

    // Otherwise, we'll listen to both content's resize event and browser's load end;
    // since a racing condition can happen, where the content's listener is added after
    // the resize, because the content's document was reloaded; therefore the test would
    // hang forever. See bug 1302879.
    const browser = ui.getViewportBrowser();

    const onResize = data => {
      if (!isSizeMatching(data)) {
        return;
      }
      ui.off("content-resize", onResize);
      browser.removeEventListener("mozbrowserloadend", onBrowserLoadEnd);
      info(`Got content-resize to ${width} x ${height}`);
      resolve();
    };

    const onBrowserLoadEnd = async function() {
      const data = await getContentSize(ui);
      onResize(data);
    };

    info(`Waiting for content-resize to ${width} x ${height}`);
    ui.on("content-resize", onResize);
    browser.addEventListener("mozbrowserloadend",
      onBrowserLoadEnd, { once: true });
  });
}

var setViewportSize = async function(ui, manager, width, height) {
  const size = ui.getViewportSize();
  info(`Current size: ${size.width} x ${size.height}, ` +
       `set to: ${width} x ${height}`);
  if (size.width != width || size.height != height) {
    const resized = waitForViewportResizeTo(ui, width, height);
    ui.setViewportSize({ width, height });
    await resized;
  }
};

function getViewportDevicePixelRatio(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    return content.devicePixelRatio;
  });
}

function getElRect(selector, win) {
  const el = win.document.querySelector(selector);
  return el.getBoundingClientRect();
}

/**
 * Drag an element identified by 'selector' by [x,y] amount. Returns
 * the rect of the dragged element as it was before drag.
 */
function dragElementBy(selector, x, y, win) {
  const { Simulate } = win.require("devtools/client/shared/vendor/react-dom-test-utils");
  const rect = getElRect(selector, win);
  const startPoint = {
    clientX: Math.floor(rect.left + rect.width / 2),
    clientY: Math.floor(rect.top + rect.height / 2),
  };
  const endPoint = [ startPoint.clientX + x, startPoint.clientY + y ];

  const elem = win.document.querySelector(selector);

  // mousedown is a React listener, need to use its testing tools to avoid races
  Simulate.mouseDown(elem, startPoint);

  // mousemove and mouseup are regular DOM listeners
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mousemove" }, win);
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mouseup" }, win);

  return rect;
}

async function testViewportResize(ui, selector, moveBy,
                             expectedViewportSize, expectedHandleMove) {
  const win = ui.toolWindow;
  const resized = waitForViewportResizeTo(ui, ...expectedViewportSize);
  const startRect = dragElementBy(selector, ...moveBy, win);
  await resized;

  const endRect = getElRect(selector, win);
  is(endRect.left - startRect.left, expectedHandleMove[0],
    `The x move of ${selector} is as expected`);
  is(endRect.top - startRect.top, expectedHandleMove[1],
    `The y move of ${selector} is as expected`);
}

function openDeviceModal({ toolWindow }) {
  const { document } = toolWindow;
  const { Simulate } = toolWindow.require("devtools/client/shared/vendor/react-dom-test-utils");
  const select = document.querySelector(".viewport-device-selector");
  const modal = document.querySelector("#device-modal-wrapper");

  info("Checking initial device modal state");
  ok(modal.classList.contains("closed") && !modal.classList.contains("opened"),
    "The device modal is closed by default.");

  info("Opening device modal through device selector.");
  select.value = OPEN_DEVICE_MODAL_VALUE;
  Simulate.change(select);
  ok(modal.classList.contains("opened") && !modal.classList.contains("closed"),
    "The device modal is displayed.");
}

function changeSelectValue({ toolWindow }, selector, value) {
  const { document } = toolWindow;
  const { Simulate } =
    toolWindow.require("devtools/client/shared/vendor/react-dom-test-utils");

  info(`Selecting ${value} in ${selector}.`);

  const select = document.querySelector(selector);
  isnot(select, null, `selector "${selector}" should match an existing element.`);

  const option = [...select.options].find(o => o.value === String(value));
  isnot(option, undefined, `value "${value}" should match an existing option.`);

  select.value = value;
  Simulate.change(select);
}

const selectDevice = (ui, value) => Promise.all([
  once(ui, "device-changed"),
  changeSelectValue(ui, ".viewport-device-selector", value)
]);

const selectDevicePixelRatio = (ui, value) =>
  changeSelectValue(ui, "#global-device-pixel-ratio-selector", value);

const selectNetworkThrottling = (ui, value) => Promise.all([
  once(ui, "network-throttling-changed"),
  changeSelectValue(ui, "#global-network-throttling-selector", value)
]);

function getSessionHistory(browser) {
  return ContentTask.spawn(browser, {}, async function() {
    /* eslint-disable no-undef */
    const { SessionHistory } =
      ChromeUtils.import("resource://gre/modules/sessionstore/SessionHistory.jsm", {});
    return SessionHistory.collect(docShell);
    /* eslint-enable no-undef */
  });
}

function getContentSize(ui) {
  return spawnViewportTask(ui, {}, () => ({
    width: content.screen.width,
    height: content.screen.height
  }));
}

async function waitForPageShow(browser) {
  const tab = gBrowser.getTabForBrowser(browser);
  const ui = ResponsiveUIManager.getResponsiveUIForTab(tab);
  if (ui) {
    browser = ui.getViewportBrowser();
  }
  info("Waiting for pageshow from " + (ui ? "responsive" : "regular") + " browser");
  // Need to wait an extra tick after pageshow to ensure everyone is up-to-date,
  // hence the waitForTick.
  await BrowserTestUtils.waitForContentEvent(browser, "pageshow");
  return waitForTick();
}

function waitForViewportLoad(ui) {
  return BrowserTestUtils.waitForContentEvent(ui.getViewportBrowser(), "load", true);
}

function load(browser, url) {
  const loaded = BrowserTestUtils.browserLoaded(browser, false, url);
  browser.loadURI(url);
  return loaded;
}

function back(browser) {
  const shown = waitForPageShow(browser);
  browser.goBack();
  return shown;
}

function forward(browser) {
  const shown = waitForPageShow(browser);
  browser.goForward();
  return shown;
}

function addDeviceForTest(device) {
  info(`Adding Test Device "${device.name}" to the list.`);
  addDevice(device);

  registerCleanupFunction(() => {
    // Note that assertions in cleanup functions are not displayed unless they failed.
    ok(removeDevice(device), `Removed Test Device "${device.name}" from the list.`);
  });
}

async function waitForClientClose(ui) {
  info("Waiting for RDM debugger client to close");
  await ui.client.addOneTimeListener("closed");
  info("RDM's debugger client is now closed");
}

async function testTouchEventsOverride(ui, expected) {
  const { document } = ui.toolWindow;
  const touchButton = document.querySelector("#global-touch-simulation-button");

  const flag = await ui.emulationFront.getTouchEventsOverride();
  is(flag === Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_ENABLED, expected,
    `Touch events override should be ${expected ? "enabled" : "disabled"}`);
  is(touchButton.classList.contains("checked"), expected,
    `Touch simulation button should be ${expected ? "" : "in"}active.`);
}

function testViewportDeviceSelectLabel(ui, expected) {
  info("Test viewport's device select label");

  const select = ui.toolWindow.document.querySelector(".viewport-device-selector");
  is(select.selectedOptions[0].textContent, expected,
     `Device Select value should be: ${expected}`);
}

async function toggleTouchSimulation(ui) {
  const { document } = ui.toolWindow;
  const touchButton = document.querySelector("#global-touch-simulation-button");
  const changed = once(ui, "touch-simulation-changed");
  const loaded = waitForViewportLoad(ui);
  touchButton.click();
  await Promise.all([ changed, loaded ]);
}

function testUserAgent(ui, expected) {
  testUserAgentFromBrowser(ui.getViewportBrowser(), expected);
}

async function testUserAgentFromBrowser(browser, expected) {
  const ua = await ContentTask.spawn(browser, {}, async function() {
    return content.navigator.userAgent;
  });
  is(ua, expected, `UA should be set to ${expected}`);
}

/**
 * Assuming the device modal is open and the device adder form is shown, this helper
 * function adds `device` via the form, saves it, and waits for it to appear in the store.
 */
function addDeviceInModal(ui, device) {
  const { Simulate } =
    ui.toolWindow.require("devtools/client/shared/vendor/react-dom-test-utils");
  const { store, document } = ui.toolWindow;

  const nameInput = document.querySelector("#device-adder-name input");
  const [ widthInput, heightInput ] =
    document.querySelectorAll("#device-adder-size input");
  const pixelRatioInput = document.querySelector("#device-adder-pixel-ratio input");
  const userAgentInput = document.querySelector("#device-adder-user-agent input");
  const touchInput = document.querySelector("#device-adder-touch input");

  nameInput.value = device.name;
  Simulate.change(nameInput);
  widthInput.value = device.width;
  Simulate.change(widthInput);
  Simulate.blur(widthInput);
  heightInput.value = device.height;
  Simulate.change(heightInput);
  Simulate.blur(heightInput);
  pixelRatioInput.value = device.pixelRatio;
  Simulate.change(pixelRatioInput);
  userAgentInput.value = device.userAgent;
  Simulate.change(userAgentInput);
  touchInput.checked = device.touch;
  Simulate.change(touchInput);

  const existingCustomDevices = store.getState().devices.custom.length;
  const adderSave = document.querySelector("#device-adder-save");
  const saved = waitUntilState(store, state =>
    state.devices.custom.length == existingCustomDevices + 1
  );
  Simulate.click(adderSave);
  return saved;
}

function reloadOnUAChange(enabled) {
  const pref = RELOAD_CONDITION_PREF_PREFIX + "userAgent";
  Services.prefs.setBoolPref(pref, enabled);
}

function reloadOnTouchChange(enabled) {
  const pref = RELOAD_CONDITION_PREF_PREFIX + "touchSimulation";
  Services.prefs.setBoolPref(pref, enabled);
}
