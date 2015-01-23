/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
let { Promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});
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

gDevTools.testing = true;

let DEFAULT_PREFS = [
  "devtools.debugger.log",
  "devtools.performance.ui.invert-call-tree",
  // remove after bug 1075567 is resolved.
  "devtools.performance_dev.enabled"
].reduce((prefs, pref) => {
  prefs[pref] = Services.prefs.getBoolPref(pref);
  return prefs;
}, {});

// Enable the new performance panel for all tests. Remove this after
// bug 1075567 is resolved.
Services.prefs.setBoolPref("devtools.performance_dev.enabled", true);
// Enable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
Services.prefs.setBoolPref("devtools.debugger.log", false);

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

  // Rollback any pref changes
  Object.keys(DEFAULT_PREFS).forEach(pref => {
    Services.prefs.setBoolPref(pref, DEFAULT_PREFS[pref]);
  });

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

function once(aTarget, aEventName, aUseCapture = false, spread = false) {
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
        deferred.resolve(spread ? aArgs : aArgs[0]);
      }, aUseCapture);
      break;
    }
  }

  return deferred.promise;
}

/**
 * Like `once`, except returns an array so we can
 * access all arguments fired by the event.
 */
function onceSpread(aTarget, aEventName, aUseCapture) {
  return once(aTarget, aEventName, aUseCapture, true);
}

function test () {
  Task.spawn(spawnTest).then(finish, handleError);
}

function initBackend(aUrl) {
  info("Initializing a performance front.");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
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

function initPerformance(aUrl, selectedTool="performance") {
  info("Initializing a performance pane.");

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    let toolbox = yield gDevTools.showToolbox(target, selectedTool);
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

function busyWait(time) {
  let start = Date.now();
  let stack;
  while (Date.now() - start < time) { stack = Components.stack; }
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

function command (button) {
  let ev = button.ownerDocument.createEvent("XULCommandEvent");
  ev.initCommandEvent("command", true, true, button.ownerDocument.defaultView, 0, false, false, false, false, null);
  button.dispatchEvent(ev);
}

function click (win, button) {
  EventUtils.sendMouseEvent({ type: "click" }, button, win);
}

function* startRecording(panel) {
  let win = panel.panelWin;
  let clicked = panel.panelWin.PerformanceView.once(win.EVENTS.UI_START_RECORDING);
  let willStart = panel.panelWin.PerformanceController.once(win.EVENTS.RECORDING_WILL_START);
  let hasStarted = panel.panelWin.PerformanceController.once(win.EVENTS.RECORDING_STARTED);
  let button = win.$("#record-button");

  ok(!button.hasAttribute("checked"),
    "The record button should not be checked yet.");

  ok(!button.hasAttribute("locked"),
    "The record button should not be locked yet.");

  click(win, button);

  yield clicked;

  ok(button.hasAttribute("checked"),
    "The record button should now be checked.");
  ok(button.hasAttribute("locked"),
    "The record button should be locked.");

  yield willStart;
  yield hasStarted;

  ok(button.hasAttribute("checked"),
    "The record button should still be checked.");
  ok(!button.hasAttribute("locked"),
    "The record button should not be locked.");
}

function* stopRecording(panel) {
  let win = panel.panelWin;
  let clicked = panel.panelWin.PerformanceView.once(win.EVENTS.UI_STOP_RECORDING);
  let willStop = panel.panelWin.PerformanceController.once(win.EVENTS.RECORDING_WILL_STOP);
  let hasStopped = panel.panelWin.PerformanceController.once(win.EVENTS.RECORDING_STOPPED);
  let button = win.$("#record-button");

  ok(button.hasAttribute("checked"),
    "The record button should already be checked.");
  ok(!button.hasAttribute("locked"),
    "The record button should not be locked yet.");

  click(win, button);

  yield clicked;

  ok(!button.hasAttribute("checked"),
    "The record button should not be checked.");
  ok(button.hasAttribute("locked"),
    "The record button should be locked.");

  yield willStop;
  yield hasStopped;

  ok(!button.hasAttribute("checked"),
    "The record button should not be checked.");
  ok(!button.hasAttribute("locked"),
    "The record button should not be locked.");
}

function waitForWidgetsRendered(panel) {
  let {
    EVENTS,
    OverviewView,
    CallTreeView,
    WaterfallView,
    FlameGraphView
  } = panel.panelWin;

  return Promise.all([
    once(OverviewView, EVENTS.MARKERS_GRAPH_RENDERED),
    once(OverviewView, EVENTS.MEMORY_GRAPH_RENDERED),
    once(OverviewView, EVENTS.FRAMERATE_GRAPH_RENDERED),
    once(OverviewView, EVENTS.OVERVIEW_RENDERED),
    once(WaterfallView, EVENTS.WATERFALL_RENDERED),
    once(CallTreeView, EVENTS.CALL_TREE_RENDERED),
    once(FlameGraphView, EVENTS.FLAMEGRAPH_RENDERED)
  ]);
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
    return Promise.resolve(true);
  }
  let deferred = Promise.defer();
  setTimeout(function() {
    waitUntil(predicate).then(() => deferred.resolve(true));
  }, interval);
  return deferred.promise;
}

// EventUtils just doesn't work!

function dragStart(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ clientX: x, clientY: y });
  graph._onMouseDown({ clientX: x, clientY: y });
}

function dragStop(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ clientX: x, clientY: y });
  graph._onMouseUp({ clientX: x, clientY: y });
}

function dropSelection(graph) {
  graph.dropSelection();
  graph.emit("selecting");
}

function getSourceActor(aSources, aURL) {
  let item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item && item.value;
}
