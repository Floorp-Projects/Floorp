/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../framework/test/shared-head.js */
/* import-globals-from ../../../framework/test/shared-redux-head.js */
/* import-globals-from ../../../commandline/test/helpers.js */
/* import-globals-from ../../../inspector/test/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js",
  this);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/shared-redux-head.js",
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

const { _loadPreferredDevices } = require("devtools/client/responsive.html/actions/devices");
const { getOwnerWindow } = require("sdk/tabs/utils");
const asyncStorage = require("devtools/shared/async-storage");
const { addDevice, removeDevice } = require("devtools/client/shared/devices");

SimpleTest.requestCompleteLog();
SimpleTest.waitForExplicitFinish();

// Toggling the RDM UI involves several docShell swap operations, which are somewhat slow
// on debug builds. Usually we are just barely over the limit, so a blanket factor of 2
// should be enough.
requestLongerTimeout(2);

flags.testing = true;
Services.prefs.clearUserPref("devtools.responsive.html.displayedDeviceList");
Services.prefs.setCharPref("devtools.devices.url",
  TEST_URI_ROOT + "devices.json");
Services.prefs.setBoolPref("devtools.responsive.html.enabled", true);

registerCleanupFunction(() => {
  flags.testing = false;
  Services.prefs.clearUserPref("devtools.devices.url");
  Services.prefs.clearUserPref("devtools.responsive.html.enabled");
  Services.prefs.clearUserPref("devtools.responsive.html.displayedDeviceList");
  asyncStorage.removeItem("devtools.devices.url_cache");
});

// This depends on the "devtools.responsive.html.enabled" pref
const { ResponsiveUIManager } = require("resource://devtools/client/responsivedesign/responsivedesign.jsm");

/**
 * Open responsive design mode for the given tab.
 */
var openRDM = Task.async(function* (tab) {
  info("Opening responsive design mode");
  let manager = ResponsiveUIManager;
  let ui = yield manager.openIfNeeded(getOwnerWindow(tab), tab);
  info("Responsive design mode opened");
  return { ui, manager };
});

/**
 * Close responsive design mode for the given tab.
 */
var closeRDM = Task.async(function* (tab, options) {
  info("Closing responsive design mode");
  let manager = ResponsiveUIManager;
  yield manager.closeIfNeeded(getOwnerWindow(tab), tab, options);
  info("Responsive design mode closed");
});

/**
 * Adds a new test task that adds a tab with the given URL, opens responsive
 * design mode, runs the given generator, closes responsive design mode, and
 * removes the tab.
 *
 * Example usage:
 *
 *   addRDMTask(TEST_URL, function*({ ui, manager }) {
 *     // Your tests go here...
 *   });
 */
function addRDMTask(url, generator) {
  add_task(function* () {
    const tab = yield addTab(url);
    const results = yield openRDM(tab);

    try {
      yield* generator(results);
    } catch (err) {
      ok(false, "Got an error: " + DevToolsUtils.safeErrorString(err));
    }

    yield closeRDM(tab);
    yield removeTab(tab);
  });
}

function spawnViewportTask(ui, args, task) {
  return ContentTask.spawn(ui.getViewportBrowser(), args, task);
}

function waitForFrameLoad(ui, targetURL) {
  return spawnViewportTask(ui, { targetURL }, function* (args) {
    if ((content.document.readyState == "complete" ||
         content.document.readyState == "interactive") &&
        content.location.href == args.targetURL) {
      return;
    }
    yield ContentTaskUtils.waitForEvent(this, "DOMContentLoaded");
  });
}

function waitForViewportResizeTo(ui, width, height) {
  return new Promise(Task.async(function* (resolve) {
    let isSizeMatching = (data) => data.width == width && data.height == height;

    // If the viewport has already the expected size, we resolve the promise immediately.
    let size = yield getContentSize(ui);
    if (isSizeMatching(size)) {
      resolve();
      return;
    }

    // Otherwise, we'll listen to both content's resize event and browser's load end;
    // since a racing condition can happen, where the content's listener is added after
    // the resize, because the content's document was reloaded; therefore the test would
    // hang forever. See bug 1302879.
    let browser = ui.getViewportBrowser();

    let onResize = (_, data) => {
      if (!isSizeMatching(data)) {
        return;
      }
      ui.off("content-resize", onResize);
      browser.removeEventListener("mozbrowserloadend", onBrowserLoadEnd);
      info(`Got content-resize to ${width} x ${height}`);
      resolve();
    };

    let onBrowserLoadEnd = Task.async(function* () {
      let data = yield getContentSize(ui);
      onResize(undefined, data);
    });

    info(`Waiting for content-resize to ${width} x ${height}`);
    ui.on("content-resize", onResize);
    browser.addEventListener("mozbrowserloadend",
      onBrowserLoadEnd, { once: true });
  }));
}

var setViewportSize = Task.async(function* (ui, manager, width, height) {
  let size = ui.getViewportSize();
  info(`Current size: ${size.width} x ${size.height}, ` +
       `set to: ${width} x ${height}`);
  if (size.width != width || size.height != height) {
    let resized = waitForViewportResizeTo(ui, width, height);
    ui.setViewportSize({ width, height });
    yield resized;
  }
});

function getElRect(selector, win) {
  let el = win.document.querySelector(selector);
  return el.getBoundingClientRect();
}

/**
 * Drag an element identified by 'selector' by [x,y] amount. Returns
 * the rect of the dragged element as it was before drag.
 */
function dragElementBy(selector, x, y, win) {
  let React = win.require("devtools/client/shared/vendor/react");
  let { Simulate } = React.addons.TestUtils;
  let rect = getElRect(selector, win);
  let startPoint = {
    clientX: rect.left + Math.floor(rect.width / 2),
    clientY: rect.top + Math.floor(rect.height / 2),
  };
  let endPoint = [ startPoint.clientX + x, startPoint.clientY + y ];

  let elem = win.document.querySelector(selector);

  // mousedown is a React listener, need to use its testing tools to avoid races
  Simulate.mouseDown(elem, startPoint);

  // mousemove and mouseup are regular DOM listeners
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mousemove" }, win);
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mouseup" }, win);

  return rect;
}

function* testViewportResize(ui, selector, moveBy,
                             expectedViewportSize, expectedHandleMove) {
  let win = ui.toolWindow;

  let changed = once(ui, "viewport-device-changed");
  let resized = waitForViewportResizeTo(ui, ...expectedViewportSize);
  let startRect = dragElementBy(selector, ...moveBy, win);
  yield resized;
  yield changed;

  let endRect = getElRect(selector, win);
  is(endRect.left - startRect.left, expectedHandleMove[0],
    `The x move of ${selector} is as expected`);
  is(endRect.top - startRect.top, expectedHandleMove[1],
    `The y move of ${selector} is as expected`);
}

function openDeviceModal({ toolWindow }) {
  let { document } = toolWindow;
  let React = toolWindow.require("devtools/client/shared/vendor/react");
  let { Simulate } = React.addons.TestUtils;
  let select = document.querySelector(".viewport-device-selector");
  let modal = document.querySelector("#device-modal-wrapper");

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
  info(`Selecting ${value} in ${selector}.`);

  return new Promise(resolve => {
    let select = toolWindow.document.querySelector(selector);
    isnot(select, null, `selector "${selector}" should match an existing element.`);

    let option = [...select.options].find(o => o.value === String(value));
    isnot(option, undefined, `value "${value}" should match an existing option.`);

    let event = new toolWindow.UIEvent("change", {
      view: toolWindow,
      bubbles: true,
      cancelable: true
    });

    select.addEventListener("change", () => {
      is(select.value, value,
        `Select's option with value "${value}" should be selected.`);
      resolve();
    }, { once: true });

    select.value = value;
    select.dispatchEvent(event);
  });
}

const selectDevice = (ui, value) => Promise.all([
  once(ui, "viewport-device-changed"),
  changeSelectValue(ui, ".viewport-device-selector", value)
]);

const selectDPR = (ui, value) =>
  changeSelectValue(ui, "#global-dpr-selector > select", value);

const selectNetworkThrottling = (ui, value) => Promise.all([
  once(ui, "network-throttling-changed"),
  changeSelectValue(ui, "#global-network-throttling-selector", value)
]);

function getSessionHistory(browser) {
  return ContentTask.spawn(browser, {}, function* () {
    /* eslint-disable no-undef */
    let { interfaces: Ci } = Components;
    let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    let sessionHistory = webNav.sessionHistory;
    let result = {
      index: sessionHistory.index,
      entries: []
    };

    for (let i = 0; i < sessionHistory.count; i++) {
      let entry = sessionHistory.getEntryAtIndex(i, false);
      result.entries.push({
        uri: entry.URI.spec,
        title: entry.title
      });
    }

    return result;
    /* eslint-enable no-undef */
  });
}

function getContentSize(ui) {
  return spawnViewportTask(ui, {}, () => ({
    width: content.screen.width,
    height: content.screen.height
  }));
}

function waitForPageShow(browser) {
  let mm = browser.messageManager;
  return new Promise(resolve => {
    let onShow = message => {
      if (message.target != browser) {
        return;
      }
      mm.removeMessageListener("PageVisibility:Show", onShow);
      resolve();
    };
    mm.addMessageListener("PageVisibility:Show", onShow);
  });
}

function load(browser, url) {
  let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
  browser.loadURI(url, null, null);
  return loaded;
}

function back(browser) {
  let shown = waitForPageShow(browser);
  browser.goBack();
  return shown;
}

function forward(browser) {
  let shown = waitForPageShow(browser);
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

function waitForClientClose(ui) {
  return new Promise(resolve => {
    info("RDM's debugger client is now closed");
    ui.client.addOneTimeListener("closed", resolve);
  });
}
