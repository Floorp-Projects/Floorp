/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

// This shared-head.js file is used for multiple mochitest test directories in
// devtools.
// It contains various common helper functions.

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr, Constructor: CC}
  = Components;

function scopedCuImport(path) {
  const scope = {};
  ChromeUtils.import(path, scope);
  return scope;
}

const {ScratchpadManager} = scopedCuImport("resource://devtools/client/scratchpad/scratchpad-manager.jsm");
const {loader, require} = scopedCuImport("resource://devtools/shared/Loader.jsm");

const {gDevTools} = require("devtools/client/framework/devtools");
const {TargetFactory} = require("devtools/client/framework/target");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
let promise = require("promise");
let defer = require("devtools/shared/defer");
const Services = require("Services");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";
const URL_ROOT = CHROME_URL_ROOT.replace("chrome://mochitests/content/",
                                         "http://example.com/");
const URL_ROOT_SSL = CHROME_URL_ROOT.replace("chrome://mochitests/content/",
                                             "https://example.com/");

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/telemetry-test-helpers.js", this);

// Force devtools to be initialized so menu items and keyboard shortcuts get installed
require("devtools/client/framework/devtools-browser");

// All test are asynchronous
waitForExplicitFinish();

var EXPECTED_DTU_ASSERT_FAILURE_COUNT = 0;

registerCleanupFunction(function() {
  if (DevToolsUtils.assertionFailureCount !==
      EXPECTED_DTU_ASSERT_FAILURE_COUNT) {
    ok(false,
      "Should have had the expected number of DevToolsUtils.assert() failures."
      + " Expected " + EXPECTED_DTU_ASSERT_FAILURE_COUNT
      + ", got " + DevToolsUtils.assertionFailureCount);
  }
});

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

/**
 * Watch console messages for failed propType definitions in React components.
 */
const ConsoleObserver = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  observe: function(subject) {
    let message = subject.wrappedJSObject.arguments[0];

    if (message && /Failed propType/.test(message.toString())) {
      ok(false, message);
    }
  }
};

Services.obs.addObserver(ConsoleObserver, "console-api-log-event");
registerCleanupFunction(() => {
  Services.obs.removeObserver(ConsoleObserver, "console-api-log-event");
});

// Print allocation count if DEBUG_DEVTOOLS_ALLOCATIONS is set to "normal",
// and allocation sites if DEBUG_DEVTOOLS_ALLOCATIONS is set to "verbose".
const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
const DEBUG_ALLOCATIONS = env.get("DEBUG_DEVTOOLS_ALLOCATIONS");
if (DEBUG_ALLOCATIONS) {
  const { allocationTracker } = require("devtools/shared/test-helpers/allocation-tracker");
  let tracker = allocationTracker();
  registerCleanupFunction(() => {
    if (DEBUG_ALLOCATIONS == "normal") {
      tracker.logCount();
    } else if (DEBUG_ALLOCATIONS == "verbose") {
      tracker.logAllocationSites();
    }
    tracker.stop();
  });
}

var waitForTime = DevToolsUtils.waitForTime;

function loadFrameScriptUtils(browser = gBrowser.selectedBrowser) {
  let mm = browser.messageManager;
  let frameURL = "chrome://mochitests/content/browser/devtools/client/shared/test/frame-script-utils.js";
  info("Loading the helper frame script " + frameURL);
  mm.loadFrameScript(frameURL, false);
  SimpleTest.registerCleanupFunction(() => {
    mm = null;
  });
  return mm;
}

Services.prefs.setBoolPref("devtools.inspector.show-three-pane-tooltip", false);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.inspector.show-three-pane-tooltip");
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.previousHost");
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleEnabled");
  Services.prefs.clearUserPref("devtools.toolbox.splitconsoleHeight");
});

registerCleanupFunction(async function cleanup() {
  while (gBrowser.tabs.length > 1) {
    await closeTabAndToolbox(gBrowser.selectedTab);
  }
});

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @param {Object} options Object with various optional fields:
 *   - {Boolean} background If true, open the tab in background
 *   - {ChromeWindow} window Firefox top level window we should use to open the tab
 *   - {Number} userContextId The userContextId of the tab.
 *   - {String} preferredRemoteType
 * @return a promise that resolves to the tab object when the url is loaded
 */
var addTab = async function(url, options = { background: false, window: window }) {
  info("Adding a new tab with URL: " + url);

  let { background } = options;
  let { gBrowser } = options.window ? options.window : window;
  let { userContextId } = options;

  let tab = BrowserTestUtils.addTab(gBrowser, url,
    {userContextId, preferredRemoteType: options.preferredRemoteType});
  if (!background) {
    gBrowser.selectedTab = tab;
  }
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info("Tab added and finished loading");

  return tab;
};

/**
 * Remove the given tab.
 * @param {Object} tab The tab to be removed.
 * @return Promise<undefined> resolved when the tab is successfully removed.
 */
var removeTab = async function(tab) {
  info("Removing tab.");

  let { gBrowser } = tab.ownerDocument.defaultView;
  let onClose = once(gBrowser.tabContainer, "TabClose");
  gBrowser.removeTab(tab);
  await onClose;

  info("Tab removed and finished closing");
};

/**
 * Refresh the provided tab.
 * @param {Object} tab The tab to be refreshed. Defaults to the currently selected tab.
 * @return Promise<undefined> resolved when the tab is successfully refreshed.
 */
var refreshTab = async function(tab = gBrowser.selectedTab) {
  info("Refreshing tab.");
  const finished = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(tab);
  await finished;
  info("Tab finished refreshing.");
};

/**
 * Simulate a key event from a <key> element.
 * @param {DOMNode} key
 */
function synthesizeKeyFromKeyTag(key) {
  is(key && key.tagName, "key", "Successfully retrieved the <key> node");

  let modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode")) {
    name = key.getAttribute("keycode");
  } else if (key.getAttribute("key")) {
    name = key.getAttribute("key");
  }

  isnot(name, null, "Successfully retrieved keycode/key");

  let modifiers = {
    shiftKey: !!modifiersAttr.match("shift"),
    ctrlKey: !!modifiersAttr.match("control"),
    altKey: !!modifiersAttr.match("alt"),
    metaKey: !!modifiersAttr.match("meta"),
    accelKey: !!modifiersAttr.match("accel")
  };

  info("Synthesizing key " + name + " " + JSON.stringify(modifiers));
  EventUtils.synthesizeKey(name, modifiers);
}

/**
 * Simulate a key event from an electron key shortcut string:
 * https://github.com/electron/electron/blob/master/docs/api/accelerator.md
 *
 * @param {String} key
 * @param {DOMWindow} target
 *        Optional window where to fire the key event
 */
function synthesizeKeyShortcut(key, target) {
  // parseElectronKey requires any window, just to access `KeyboardEvent`
  let window = Services.appShell.hiddenDOMWindow;
  let shortcut = KeyShortcuts.parseElectronKey(window, key);
  let keyEvent = {
    altKey: shortcut.alt,
    ctrlKey: shortcut.ctrl,
    metaKey: shortcut.meta,
    shiftKey: shortcut.shift
  };
  if (shortcut.keyCode) {
    keyEvent.keyCode = shortcut.keyCode;
  }

  info("Synthesizing key shortcut: " + key);
  EventUtils.synthesizeKey(shortcut.key || "", keyEvent, target);
}

/**
 * Wait for eventName on target to be delivered a number of times.
 *
 * @param {Object} target
 *        An observable object that either supports on/off or
 *        addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Number} numTimes
 *        Number of deliveries to wait for.
 * @param {Boolean} useCapture
 *        Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function waitForNEvents(target, eventName, numTimes, useCapture = false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  let count = 0;

  return new Promise(resolve => {
    for (let [add, remove] of [
      ["on", "off"],
      ["addEventListener", "removeEventListener"],
      ["addListener", "removeListener"],
    ]) {
      if ((add in target) && (remove in target)) {
        target[add](eventName, function onEvent(...args) {
          if (typeof info === "function") {
            info("Got event: '" + eventName + "' on " + target + ".");
          }

          if (++count == numTimes) {
            target[remove](eventName, onEvent, useCapture);
            resolve(...args);
          }
        }, useCapture);
        break;
      }
    }
  });
}

/**
 * Wait for DOM change on target.
 *
 * @param {Object} target
 *        The Node on which to observe DOM mutations.
 * @param {String} selector
 *        Given a selector to watch whether the expected element is changed
 *        on target.
 * @param {Number} expectedLength
 *        Optional, default set to 1
 *        There may be more than one element match an array match the selector,
 *        give an expected length to wait for more elements.
 * @return A promise that resolves when the event has been handled
 */
function waitForDOM(target, selector, expectedLength = 1) {
  return new Promise((resolve) => {
    let observer = new MutationObserver((mutations) => {
      mutations.forEach((mutation) => {
        let elements = mutation.target.querySelectorAll(selector);

        if (elements.length === expectedLength) {
          observer.disconnect();
          resolve(elements);
        }
      });
    });

    observer.observe(target, {
      attributes: true,
      childList: true,
      subtree: true,
    });
  });
}

/**
 * Wait for eventName on target.
 *
 * @param {Object} target
 *        An observable object that either supports on/off or
 *        addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture
 *        Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture = false) {
  return waitForNEvents(target, eventName, 1, useCapture);
}

/**
 * Some tests may need to import one or more of the test helper scripts.
 * A test helper script is simply a js file that contains common test code that
 * is either not common-enough to be in head.js, or that is located in a
 * separate directory.
 * The script will be loaded synchronously and in the test's scope.
 * @param {String} filePath The file path, relative to the current directory.
 *                 Examples:
 *                 - "helper_attributes_test_runner.js"
 *                 - "../../../commandline/test/helpers.js"
 */
function loadHelperScript(filePath) {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/" + filePath, this);
}

/**
 * Wait for a tick.
 * @return {Promise}
 */
function waitForTick() {
  return new Promise(resolve => executeSoon(resolve));
}

/**
 * This shouldn't be used in the tests, but is useful when writing new tests or
 * debugging existing tests in order to introduce delays in the test steps
 *
 * @param {Number} ms
 *        The time to wait
 * @return A promise that resolves when the time is passed
 */
function wait(ms) {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
    info("Waiting " + ms / 1000 + " seconds.");
  });
}

/**
 * Open the toolbox in a given tab.
 * @param {XULNode} tab The tab the toolbox should be opened in.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves with the toolbox, when it has been opened.
 */
var openToolboxForTab = async function(tab, toolId, hostType) {
  info("Opening the toolbox");

  let toolbox;
  let target = TargetFactory.forTab(tab);
  await target.makeRemote();

  // Check if the toolbox is already loaded.
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    if (!toolId || (toolId && toolbox.getPanel(toolId))) {
      info("Toolbox is already opened");
      return toolbox;
    }
  }

  // If not, load it now.
  toolbox = await gDevTools.showToolbox(target, toolId, hostType);

  // Make sure that the toolbox frame is focused.
  await new Promise(resolve => waitForFocus(resolve, toolbox.win));

  info("Toolbox opened and focused");

  return toolbox;
};

/**
 * Add a new tab and open the toolbox in it.
 * @param {String} url The URL for the tab to be opened.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves when the tab has been added, loaded and the
 * toolbox has been opened. Resolves to the toolbox.
 */
var openNewTabAndToolbox = async function(url, toolId, hostType) {
  let tab = await addTab(url);
  return openToolboxForTab(tab, toolId, hostType);
};

/**
 * Close a tab and if necessary, the toolbox that belongs to it
 * @param {Tab} tab The tab to close.
 * @return {Promise} Resolves when the toolbox and tab have been destroyed and
 * closed.
 */
var closeTabAndToolbox = async function(tab = gBrowser.selectedTab) {
  let target = TargetFactory.forTab(tab);
  if (target) {
    await gDevTools.closeToolbox(target);
  }

  await removeTab(tab);
};

/**
 * Close a toolbox and the current tab.
 * @param {Toolbox} toolbox The toolbox to close.
 * @return {Promise} Resolves when the toolbox and tab have been destroyed and
 * closed.
 */
var closeToolboxAndTab = async function(toolbox) {
  await toolbox.destroy();
  await removeTab(gBrowser.selectedTab);
};

/**
 * Waits until a predicate returns true.
 *
 * @param function predicate
 *        Invoked once in a while until it returns true.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 */
function waitUntil(predicate, interval = 10) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => {
    setTimeout(function() {
      waitUntil(predicate, interval).then(() => resolve(true));
    }, interval);
  });
}

/**
 * Variant of waitUntil that accepts a predicate returning a promise.
 */
async function asyncWaitUntil(predicate, interval = 10) {
  let success = await predicate();
  while (!success) {
    // Wait for X milliseconds.
    await new Promise(resolve => setTimeout(resolve, interval));
    // Test the predicate again.
    success = await predicate();
  }
}

/**
 * Takes a string `script` and evaluates it directly in the content
 * in potentially a different process.
 */
let MM_INC_ID = 0;
function evalInDebuggee(script, browser = gBrowser.selectedBrowser) {
  return new Promise(resolve => {
    let id = MM_INC_ID++;
    let mm = browser.messageManager;
    mm.sendAsyncMessage("devtools:test:eval", { script, id });
    mm.addMessageListener("devtools:test:eval:response", handler);

    function handler({ data }) {
      if (id !== data.id) {
        return;
      }

      info(`Successfully evaled in debuggee: ${script}`);
      mm.removeMessageListener("devtools:test:eval:response", handler);
      resolve(data.value);
    }
  });
}

/**
 * Wait for a context menu popup to open.
 *
 * @param Element popup
 *        The XUL popup you expect to open.
 * @param Element button
 *        The button/element that receives the contextmenu event. This is
 *        expected to open the popup.
 * @param function onShown
 *        Function to invoke on popupshown event.
 * @param function onHidden
 *        Function to invoke on popuphidden event.
 * @return object
 *         A Promise object that is resolved after the popuphidden event
 *         callback is invoked.
 */
function waitForContextMenu(popup, button, onShown, onHidden) {
  return new Promise(resolve => {
    function onPopupShown() {
      info("onPopupShown");
      popup.removeEventListener("popupshown", onPopupShown);

      onShown && onShown();

      // Use executeSoon() to get out of the popupshown event.
      popup.addEventListener("popuphidden", onPopupHidden);
      executeSoon(() => popup.hidePopup());
    }
    function onPopupHidden() {
      info("onPopupHidden");
      popup.removeEventListener("popuphidden", onPopupHidden);

      onHidden && onHidden();

      resolve(popup);
    }

    popup.addEventListener("popupshown", onPopupShown);

    info("wait for the context menu to open");
    synthesizeContextMenuEvent(button);
  });
}

function synthesizeContextMenuEvent(el) {
  el.scrollIntoView();
  let eventDetails = {type: "contextmenu", button: 2};
  EventUtils.synthesizeMouse(el, 5, 2, eventDetails, el.ownerDocument.defaultView);
}

/**
 * Promise wrapper around SimpleTest.waitForClipboard
 */
function waitForClipboardPromise(setup, expected) {
  return new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(expected, setup, resolve, reject);
  });
}

/**
 * Simple helper to push a temporary preference. Wrapper on SpecialPowers
 * pushPrefEnv that returns a promise resolving when the preferences have been
 * updated.
 *
 * @param {String} preferenceName
 *        The name of the preference to updated
 * @param {} value
 *        The preference value, type can vary
 * @return {Promise} resolves when the preferences have been updated
 */
function pushPref(preferenceName, value) {
  return new Promise(resolve => {
    let options = {"set": [[preferenceName, value]]};
    SpecialPowers.pushPrefEnv(options, resolve);
  });
}

/**
 * Lookup the provided dotted path ("prop1.subprop2.myProp") in the provided object.
 *
 * @param {Object} obj
 *        Object to expand.
 * @param {String} path
 *        Dotted path to use to expand the object.
 * @return {?} anything that is found at the provided path in the object.
 */
function lookupPath(obj, path) {
  let segments = path.split(".");
  return segments.reduce((prev, current) => prev[current], obj);
}

var closeToolbox = async function() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.closeToolbox(target);
};

/**
 * Clean the logical clipboard content. This method only clears the OS clipboard on
 * Windows (see Bug 666254).
 */
function emptyClipboard() {
  let clipboard = Services.clipboard;
  clipboard.emptyClipboard(clipboard.kGlobalClipboard);
}

/**
 * Check if the current operating system is Windows.
 */
function isWindows() {
  return Services.appinfo.OS === "WINNT";
}

/**
 * Wait for a given toolbox to get its title updated.
 */
function waitForTitleChange(toolbox) {
  return new Promise(resolve => {
    toolbox.win.parent.addEventListener("message", function onmessage(event) {
      if (event.data.name == "set-host-title") {
        toolbox.win.parent.removeEventListener("message", onmessage);
        resolve();
      }
    });
  });
}

/**
 * Create an HTTP server that can be used to simulate custom requests within
 * a test.  It is automatically cleaned up when the test ends, so no need to
 * call `destroy`.
 *
 * See https://developer.mozilla.org/en-US/docs/Httpd.js/HTTP_server_for_unit_tests
 * for more information about how to register handlers.
 *
 * The server can be accessed like:
 *
 *   const server = createTestHTTPServer();
 *   let url = "http://localhost: " + server.identity.primaryPort + "/path";
 *
 * @returns {HttpServer}
 */
function createTestHTTPServer() {
  const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js", {});
  let server = new HttpServer();

  registerCleanupFunction(async function cleanup() {
    await new Promise(resolve => server.stop(resolve));
  });

  server.start(-1);
  return server;
}

/**
 * Inject `EventUtils` helpers into ContentTask scope.
 *
 * This helper is automatically exposed to mochitest browser tests,
 * but is missing from content task scope.
 * You should call this method only once per <browser> tag
 *
 * @param {xul:browser} browser
 *        Reference to the browser in which we load content task
 */
async function injectEventUtilsInContentTask(browser) {
  await ContentTask.spawn(browser, {}, async function() {
    if ("EventUtils" in this) {
      return;
    }

    let EventUtils = this.EventUtils = {};

    EventUtils.window = {};
    EventUtils.parent = EventUtils.window;
    /* eslint-disable camelcase */
    EventUtils._EU_Ci = Ci;
    EventUtils._EU_Cc = Cc;
    /* eslint-enable camelcase */
    // EventUtils' `sendChar` function relies on the navigator to synthetize events.
    EventUtils.navigator = content.navigator;
    EventUtils.KeyboardEvent = content.KeyboardEvent;

    EventUtils.synthesizeClick = element => new Promise(resolve => {
      element.addEventListener("click", function() {
        resolve();
      }, {once: true});

      EventUtils.synthesizeMouseAtCenter(element,
        { type: "mousedown", isSynthesized: false }, content);
      EventUtils.synthesizeMouseAtCenter(element,
        { type: "mouseup", isSynthesized: false }, content);
    });

    Services.scriptloader.loadSubScript(
      "chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);
  });
}
