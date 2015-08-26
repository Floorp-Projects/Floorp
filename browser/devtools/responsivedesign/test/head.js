/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let {require} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let {TargetFactory} = require("devtools/framework/target");
let DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
let promise = require("promise");

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

DevToolsUtils.testing = true;
registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

/**
 * Open the toolbox, with the inspector tool visible.
 * @return a promise that resolves when the inspector is ready
 */
let openInspector = Task.async(function*() {
  info("Opening the inspector");
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let inspector, toolbox;

  // Checking if the toolbox and the inspector are already loaded
  // The inspector-updated event should only be waited for if the inspector
  // isn't loaded yet
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    inspector = toolbox.getPanel("inspector");
    if (inspector) {
      info("Toolbox and inspector already open");
      return {
        toolbox: toolbox,
        inspector: inspector
      };
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "inspector");
  yield waitForToolboxFrameFocus(toolbox);
  inspector = toolbox.getPanel("inspector");

  info("Waiting for the inspector to update");
  yield inspector.once("inspector-updated");

  return {
    toolbox: toolbox,
    inspector: inspector
  };
});

/**
 * Wait for the toolbox frame to receive focus after it loads
 * @param {Toolbox} toolbox
 * @return a promise that resolves when focus has been received
 */
function waitForToolboxFrameFocus(toolbox) {
  info("Making sure that the toolbox's frame is focused");
  let def = promise.defer();
  let win = toolbox.frame.contentWindow;
  waitForFocus(def.resolve, win);
  return def.promise;
}

/**
 * Open the toolbox, with the inspector tool visible, and the sidebar that
 * corresponds to the given id selected
 * @return a promise that resolves when the inspector is ready and the sidebar
 * view is visible and ready
 */
let openInspectorSideBar = Task.async(function*(id) {
  let {toolbox, inspector} = yield openInspector();

  if (!hasSideBarTab(inspector, id)) {
    info("Waiting for the " + id + " sidebar to be ready");
    yield inspector.sidebar.once(id + "-ready");
  }

  info("Selecting the " + id + " sidebar");
  inspector.sidebar.select(id);

  return {
    toolbox: toolbox,
    inspector: inspector,
    view: inspector.sidebar.getWindowForTab(id)[id].view
  };
});

/**
 * Checks whether the inspector's sidebar corresponding to the given id already
 * exists
 * @param {InspectorPanel}
 * @param {String}
 * @return {Boolean}
 */
function hasSideBarTab(inspector, id) {
  return !!inspector.sidebar.getWindowForTab(id);
}

/**
 * Open the toolbox, with the inspector tool visible, and the computed-view
 * sidebar tab selected.
 * @return a promise that resolves when the inspector is ready and the computed
 * view is visible and ready
 */
function openComputedView() {
  return openInspectorSideBar("computedview");
}

/**
 * Open the toolbox, with the inspector tool visible, and the rule-view
 * sidebar tab selected.
 * @return a promise that resolves when the inspector is ready and the rule
 * view is visible and ready
 */
function openRuleView() {
  return openInspectorSideBar("ruleview");
}


/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
let addTab = Task.async(function* (url) {
  info("Adding a new tab with URL: '" + url + "'");

  window.focus();

  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let browser = tab.linkedBrowser;

  yield once(browser, "load", true);
  info("URL '" + url + "' loading complete");

  return tab;
});

/**
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture=false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  let deferred = promise.defer();

  for (let [add, remove] of [
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in target) && (remove in target)) {
      target[add](eventName, function onEvent(...aArgs) {
        info("Got event: '" + eventName + "' on " + target + ".");
        target[remove](eventName, onEvent, useCapture);
        deferred.resolve.apply(deferred, aArgs);
      }, useCapture);
      break;
    }
  }

  return deferred.promise;
}

function wait(ms) {
  let def = promise.defer();
  setTimeout(def.resolve, ms);
  return def.promise;
}

function synthesizeKeyFromKeyTag(aKeyId) {
  let key = document.getElementById(aKeyId);
  isnot(key, null, "Successfully retrieved the <key> node");

  let modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode"))
    name = key.getAttribute("keycode");
  else if (key.getAttribute("key"))
    name = key.getAttribute("key");

  isnot(name, null, "Successfully retrieved keycode/key");

  let modifiers = {
    shiftKey: modifiersAttr.match("shift"),
    ctrlKey: modifiersAttr.match("ctrl"),
    altKey: modifiersAttr.match("alt"),
    metaKey: modifiersAttr.match("meta"),
    accelKey: modifiersAttr.match("accel")
  }

  EventUtils.synthesizeKey(name, modifiers);
}

function nextTick() {
  let def = promise.defer();
  executeSoon(() => def.resolve())
  return def.promise;
}

/**
 * Waits for the next load to complete in the current browser.
 *
 * @return promise
 */
function waitForDocLoadComplete(aBrowser=gBrowser) {
  let deferred = promise.defer();
  let progressListener = {
    onStateChange: function (webProgress, req, flags, status) {
      let docStop = Ci.nsIWebProgressListener.STATE_IS_NETWORK |
                    Ci.nsIWebProgressListener.STATE_STOP;
      info("Saw state " + flags.toString(16) + " and status " + status.toString(16));

      // When a load needs to be retargetted to a new process it is cancelled
      // with NS_BINDING_ABORTED so ignore that case
      if ((flags & docStop) == docStop && status != Cr.NS_BINDING_ABORTED) {
        aBrowser.removeProgressListener(progressListener);
        info("Browser loaded " + aBrowser.contentWindow.location);
        deferred.resolve();
      }
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference])
  };
  aBrowser.addProgressListener(progressListener);
  info("Waiting for browser load");
  return deferred.promise;
}
