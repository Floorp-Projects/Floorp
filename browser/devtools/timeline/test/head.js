/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Disable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
let gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", false);

// Enable the tool while testing.
let gToolEnabled = Services.prefs.getBoolPref("devtools.timeline.enabled");
Services.prefs.setBoolPref("devtools.timeline.enabled", true);

let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
let { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});
let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

let TargetFactory = devtools.TargetFactory;
let Toolbox = devtools.Toolbox;

const EXAMPLE_URL = "http://example.com/browser/browser/devtools/timeline/test/";
const SIMPLE_URL = EXAMPLE_URL + "doc_simple-test.html";

// All tests are asynchronous.
waitForExplicitFinish();

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.timeline.enabled", gToolEnabled);
});

function addTab(url) {
  info("Adding tab: " + url);

  let deferred = promise.defer();
  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let linkedBrowser = tab.linkedBrowser;

  linkedBrowser.addEventListener("load", function onLoad() {
    linkedBrowser.removeEventListener("load", onLoad, true);
    info("Tab added and finished loading: " + url);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(tab) {
  info("Removing tab.");

  let deferred = promise.defer();
  let tabContainer = gBrowser.tabContainer;

  tabContainer.addEventListener("TabClose", function onClose(aEvent) {
    tabContainer.removeEventListener("TabClose", onClose, false);
    info("Tab removed and finished closing.");
    deferred.resolve();
  }, false);

  gBrowser.removeTab(tab);
  return deferred.promise;
}

/**
 * Spawns a new tab and starts up a toolbox with the timeline panel
 * automatically selected.
 *
 * Must be used within a task.
 *
 * @param string url
 *        The location of the new tab to spawn.
 * @return object
 *         A promise resolved once the timeline is initialized, with the
 *         {target, panel} instances.
 */
function* initTimelinePanel(url) {
  info("Initializing a timeline pane.");

  let tab = yield addTab(url);
  let target = TargetFactory.forTab(tab);

  yield target.makeRemote();

  let toolbox = yield gDevTools.showToolbox(target, "timeline");
  let panel = toolbox.getCurrentPanel();
  return { target, panel };
}

/**
 * Closes a tab and destroys the toolbox holding a timeline panel.
 *
 * Must be used within a task.
 *
 * @param object panel
 *        The timeline panel, created by the toolbox.
 * @return object
 *         A promise resolved once the timeline, toolbox and debuggee tab
 *         are destroyed.
 */
function* teardown(panel) {
  info("Destroying the specified timeline.");

  let tab = panel.target.tab;
  yield panel._toolbox.destroy();
  yield removeTab(tab);
}

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
    return promise.resolve(true);
  }
  let deferred = promise.defer();
  setTimeout(function() {
    waitUntil(predicate).then(() => deferred.resolve(true));
  }, interval);
  return deferred.promise;
}
