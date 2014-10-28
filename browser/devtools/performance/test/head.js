/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Enable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
let gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", false);

let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
let { Promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { getPerformanceActorsConnection, PerformanceFront } = devtools.require("devtools/performance/front");
let nsIProfilerModule = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
let TargetFactory = devtools.TargetFactory;
let mm = null;

const FRAME_SCRIPT_UTILS_URL = "chrome://browser/content/devtools/frame-script-utils.js"
const EXAMPLE_URL = "http://example.com/browser/browser/devtools/performance/test/";
const SIMPLE_URL = EXAMPLE_URL + "doc_simple-test.html";

// All tests are asynchronous.
waitForExplicitFinish();

let gToolEnabled = Services.prefs.getBoolPref("devtools.performance_dev.enabled");
let gShowTimelineMemory = Services.prefs.getBoolPref("devtools.performance.ui.show-timeline-memory");

gDevTools.testing = true;

/**
 * Call manually in tests that use frame script utils after initializing
 * the tool. Must be called after initializing so we can detect
 * whether or not `content` is a CPOW or not. Call after init but before navigating
 * to different pages.
 */
function loadFrameScripts () {
  mm = gBrowser.selectedBrowser.messageManager;
  mm.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
}

registerCleanupFunction(() => {
  gDevTools.testing = false;
  info("finish() was called, cleaning up...");

  Services.prefs.setBoolPref("devtools.performance.ui.show-timeline-memory", gShowTimelineMemory);
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.performance_dev.enabled", gToolEnabled);
  // Make sure the profiler module is stopped when the test finishes.
  nsIProfilerModule.StopProfiler();

  Cu.forceGC();
});

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  let deferred = Promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  let tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);
  let linkedBrowser = tab.linkedBrowser;

  linkedBrowser.addEventListener("load", function onLoad() {
    linkedBrowser.removeEventListener("load", onLoad, true);
    info("Tab added and finished loading: " + aUrl);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  let deferred = Promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;
  let tabContainer = targetBrowser.tabContainer;

  tabContainer.addEventListener("TabClose", function onClose(aEvent) {
    tabContainer.removeEventListener("TabClose", onClose, false);
    info("Tab removed and finished closing.");
    deferred.resolve();
  }, false);

  targetBrowser.removeTab(aTab);
  return deferred.promise;
}

function handleError(aError) {
  ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  finish();
}

function once(aTarget, aEventName, aUseCapture = false) {
  info("Waiting for event: '" + aEventName + "' on " + aTarget + ".");

  let deferred = Promise.defer();

  for (let [add, remove] of [
    ["on", "off"], // Use event emitter before DOM events for consistency
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"]
  ]) {
    if ((add in aTarget) && (remove in aTarget)) {
      aTarget[add](aEventName, function onEvent(...aArgs) {
        aTarget[remove](aEventName, onEvent, aUseCapture);
        deferred.resolve(...aArgs);
      }, aUseCapture);
      break;
    }
  }

  return deferred.promise;
}

function test () {
  Task.spawn(spawnTest).then(finish, handleError);
}

function initBackend(aUrl) {
  info("Initializing a performance front.");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init(() => true);
    DebuggerServer.addBrowserActors();
  }

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    yield gDevTools.showToolbox(target, "performance");

    let connection = getPerformanceActorsConnection(target);
    yield connection.open();
    let front = new PerformanceFront(connection);
    return { target, front };
  });
}

function initPerformance(aUrl) {
  info("Initializing a performance pane.");

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    Services.prefs.setBoolPref("devtools.performance_dev.enabled", true);
    let toolbox = yield gDevTools.showToolbox(target, "performance");
    let panel = toolbox.getCurrentPanel();
    return { target, panel, toolbox };
  });
}

function* teardown(panel) {
  info("Destroying the performance tool.");

  let tab = panel.target.tab;
  yield panel._toolbox.destroy();
  yield removeTab(tab);
}

function idleWait(time) {
  return DevToolsUtils.waitForTime(time);
}

function consoleMethod (...args) {
  if (!mm) {
    throw new Error("`loadFrameScripts()` must be called before using frame scripts.");
  }
  mm.sendAsyncMessage("devtools:test:console", args);
}

function* consoleProfile(connection, label) {
  let notified = connection.once("profile");
  consoleMethod("profile", label);
  yield notified;
}

function* consoleProfileEnd(connection) {
  let notified = connection.once("profileEnd");
  consoleMethod("profileEnd");
  yield notified;
}

function busyWait(time) {
  let start = Date.now();
  let stack;
  while (Date.now() - start < time) { stack = Components.stack; }
}

function idleWait(time) {
  return DevToolsUtils.waitForTime(time);
}

