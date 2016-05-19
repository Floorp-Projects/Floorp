/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This shared-head.js file is used for multiple mochitest test directories in
// devtools.
// It contains various common helper functions.

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr, Constructor: CC}
  = Components;

function scopedCuImport(path) {
  const scope = {};
  Cu.import(path, scope);
  return scope;
}

const {console} = scopedCuImport("resource://gre/modules/Console.jsm");
const {ScratchpadManager} = scopedCuImport("resource://devtools/client/scratchpad/scratchpad-manager.jsm");
const {loader, require} = scopedCuImport("resource://devtools/shared/Loader.jsm");

const {gDevTools} = require("devtools/client/framework/devtools");
const {TargetFactory} = require("devtools/client/framework/target");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
let promise = require("promise");
const Services = require("Services");
const {Task} = require("resource://gre/modules/Task.jsm");
const {KeyShortcuts} = require("devtools/client/shared/key-shortcuts");

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";
const URL_ROOT = CHROME_URL_ROOT.replace("chrome://mochitests/content/",
                                         "http://example.com/");
const URL_ROOT_SSL = CHROME_URL_ROOT.replace("chrome://mochitests/content/",
                                             "https://example.com/");

// All test are asynchronous
waitForExplicitFinish();

var EXPECTED_DTU_ASSERT_FAILURE_COUNT = 0;

registerCleanupFunction(function () {
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
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function (subject, topic, data) {
    let message = subject.wrappedJSObject.arguments[0];

    if (/Failed propType/.test(message)) {
      ok(false, message);
    }
  }
};

Services.obs.addObserver(ConsoleObserver, "console-api-log-event", false);
registerCleanupFunction(() => {
  Services.obs.removeObserver(ConsoleObserver, "console-api-log-event");
});

var waitForTime = DevToolsUtils.waitForTime;

function getFrameScript() {
  let mm = gBrowser.selectedBrowser.messageManager;
  let frameURL = "chrome://devtools/content/shared/frame-script-utils.js";
  mm.loadFrameScript(frameURL, false);
  SimpleTest.registerCleanupFunction(() => {
    mm = null;
  });
  return mm;
}

DevToolsUtils.testing = true;
registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.previousHost");
});

registerCleanupFunction(function* cleanup() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
var addTab = Task.async(function* (url) {
  info("Adding a new tab with URL: " + url);

  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  yield once(gBrowser.selectedBrowser, "load", true);

  info("Tab added and finished loading");

  return tab;
});

/**
 * Remove the given tab.
 * @param {Object} tab The tab to be removed.
 * @return Promise<undefined> resolved when the tab is successfully removed.
 */
var removeTab = Task.async(function* (tab) {
  info("Removing tab.");

  let onClose = once(gBrowser.tabContainer, "TabClose");
  gBrowser.removeTab(tab);
  yield onClose;

  info("Tab removed and finished closing");
});

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
 */
function synthesizeKeyShortcut(key) {
  // parseElectronKey requires any window, just to access `KeyboardEvent`
  let window = Services.appShell.hiddenDOMWindow;
  let shortcut = KeyShortcuts.parseElectronKey(window, key);

  info("Synthesizing key shortcut: " + key);
  EventUtils.synthesizeKey(shortcut.key || "", {
    keyCode: shortcut.keyCode,
    altKey: shortcut.alt,
    ctrlKey: shortcut.ctrl,
    metaKey: shortcut.meta,
    shiftKey: shortcut.shift
  });
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

  let deferred = promise.defer();
  let count = 0;

  for (let [add, remove] of [
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in target) && (remove in target)) {
      target[add](eventName, function onEvent(...aArgs) {
        info("Got event: '" + eventName + "' on " + target + ".");
        if (++count == numTimes) {
          target[remove](eventName, onEvent, useCapture);
          deferred.resolve.apply(deferred, aArgs);
        }
      }, useCapture);
      break;
    }
  }

  return deferred.promise;
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
  let deferred = promise.defer();
  executeSoon(deferred.resolve);
  return deferred.promise;
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
  let def = promise.defer();
  content.setTimeout(def.resolve, ms);
  return def.promise;
}

/**
 * Open the toolbox in a given tab.
 * @param {XULNode} tab The tab the toolbox should be opened in.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves with the toolbox, when it has been opened.
 */
var openToolboxForTab = Task.async(function* (tab, toolId, hostType) {
  info("Opening the toolbox");

  let toolbox;
  let target = TargetFactory.forTab(tab);
  yield target.makeRemote();

  // Check if the toolbox is already loaded.
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    if (!toolId || (toolId && toolbox.getPanel(toolId))) {
      info("Toolbox is already opened");
      return toolbox;
    }
  }

  // If not, load it now.
  toolbox = yield gDevTools.showToolbox(target, toolId, hostType);

  // Make sure that the toolbox frame is focused.
  yield new Promise(resolve => waitForFocus(resolve, toolbox.win));

  info("Toolbox opened and focused");

  return toolbox;
});

/**
 * Add a new tab and open the toolbox in it.
 * @param {String} url The URL for the tab to be opened.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves when the tab has been added, loaded and the
 * toolbox has been opened. Resolves to the toolbox.
 */
var openNewTabAndToolbox = Task.async(function* (url, toolId, hostType) {
  let tab = yield addTab(url);
  return openToolboxForTab(tab, toolId, hostType);
});

/**
 * Close a tab and if necessary, the toolbox that belongs to it
 * @param {Tab} tab The tab to close.
 * @return {Promise} Resolves when the toolbox and tab have been destroyed and
 * closed.
 */
var closeTabAndToolbox = Task.async(function* (tab = gBrowser.selectedTab) {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  if (target) {
    yield gDevTools.closeToolbox(target);
  }

  yield removeTab(gBrowser.selectedTab);
});

/**
 * Close a toolbox and the current tab.
 * @param {Toolbox} toolbox The toolbox to close.
 * @return {Promise} Resolves when the toolbox and tab have been destroyed and
 * closed.
 */
var closeToolboxAndTab = Task.async(function* (toolbox) {
  yield toolbox.destroy();
  yield removeTab(gBrowser.selectedTab);
});

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
    setTimeout(function () {
      waitUntil(predicate, interval).then(() => resolve(true));
    }, interval);
  });
}

/**
 * Takes a string `script` and evaluates it directly in the content
 * in potentially a different process.
 */
let MM_INC_ID = 0;
function evalInDebuggee(mm, script) {
  return new Promise(function (resolve, reject) {
    let id = MM_INC_ID++;
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
 * @param nsIDOMElement popup
 *        The XUL popup you expect to open.
 * @param nsIDOMElement button
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
  let deferred = promise.defer();

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

    deferred.resolve(popup);
  }

  popup.addEventListener("popupshown", onPopupShown);

  info("wait for the context menu to open");
  button.scrollIntoView();
  let eventDetails = {type: "contextmenu", button: 2};
  EventUtils.synthesizeMouse(button, 5, 2, eventDetails,
                             button.ownerDocument.defaultView);
  return deferred.promise;
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
