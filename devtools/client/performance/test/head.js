/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
var { Preferences } = Cu.import("resource://gre/modules/Preferences.jsm", {});
var { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
var { require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
var { gDevTools } = Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm", {});
var { console } = require("resource://gre/modules/devtools/shared/Console.jsm");
var { TargetFactory } = require("devtools/client/framework/target");
var Promise = require("promise");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { DebuggerServer } = require("devtools/server/main");
var { merge } = require("sdk/util/object");
var { createPerformanceFront } = require("devtools/server/actors/performance");
var RecordingUtils = require("devtools/shared/performance/utils");
var {
  PMM_loadFrameScripts, PMM_isProfilerActive, PMM_stopProfiler,
  sendProfilerCommand, consoleMethod
} = require("devtools/shared/performance/process-communication");

var mm = null;

const FRAME_SCRIPT_UTILS_URL = "chrome://devtools/content/shared/frame-script-utils.js"
const EXAMPLE_URL = "http://example.com/browser/devtools/client/performance/test/";
const SIMPLE_URL = EXAMPLE_URL + "doc_simple-test.html";
const MARKERS_URL = EXAMPLE_URL + "doc_markers.html";
const ALLOCS_URL = EXAMPLE_URL + "doc_allocs.html";

const MEMORY_SAMPLE_PROB_PREF = "devtools.performance.memory.sample-probability";
const MEMORY_MAX_LOG_LEN_PREF = "devtools.performance.memory.max-log-length";
const PROFILER_BUFFER_SIZE_PREF = "devtools.performance.profiler.buffer-size";
const PROFILER_SAMPLE_RATE_PREF = "devtools.performance.profiler.sample-frequency-khz";

const FRAMERATE_PREF = "devtools.performance.ui.enable-framerate";
const MEMORY_PREF = "devtools.performance.ui.enable-memory";
const ALLOCATIONS_PREF = "devtools.performance.ui.enable-allocations";

const PLATFORM_DATA_PREF = "devtools.performance.ui.show-platform-data";
const IDLE_PREF = "devtools.performance.ui.show-idle-blocks";
const INVERT_PREF = "devtools.performance.ui.invert-call-tree";
const INVERT_FLAME_PREF = "devtools.performance.ui.invert-flame-graph";
const FLATTEN_PREF = "devtools.performance.ui.flatten-tree-recursion";
const JIT_PREF = "devtools.performance.ui.enable-jit-optimizations";
const EXPERIMENTAL_PREF = "devtools.performance.ui.experimental";

// All tests are asynchronous.
waitForExplicitFinish();

DevToolsUtils.testing = true;

var DEFAULT_PREFS = [
  "devtools.debugger.log",
  "devtools.performance.ui.invert-call-tree",
  "devtools.performance.ui.flatten-tree-recursion",
  "devtools.performance.ui.show-triggers-for-gc-types",
  "devtools.performance.ui.show-platform-data",
  "devtools.performance.ui.show-idle-blocks",
  "devtools.performance.ui.enable-memory",
  "devtools.performance.ui.enable-allocations",
  "devtools.performance.ui.enable-framerate",
  "devtools.performance.ui.enable-jit-optimizations",
  "devtools.performance.memory.sample-probability",
  "devtools.performance.memory.max-log-length",
  "devtools.performance.profiler.buffer-size",
  "devtools.performance.profiler.sample-frequency-khz",
  "devtools.performance.ui.experimental",
  "devtools.performance.timeline.hidden-markers",
].reduce((prefs, pref) => {
  prefs[pref] = Preferences.get(pref);
  return prefs;
}, {});

// Enable the new performance panel for all tests.
Services.prefs.setBoolPref("devtools.performance.enabled", true);
// Enable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
Services.prefs.setBoolPref("devtools.debugger.log", false);

// By default, enable memory flame graphs for tests for now
// TODO remove when we have flame charts via bug 1148663
Services.prefs.setBoolPref("devtools.performance.ui.enable-memory-flame", true);

/**
 * Call manually in tests that use frame script utils after initializing
 * the tool. Must be called after initializing (once we have a tab).
 */
function loadFrameScripts () {
  mm = gBrowser.selectedBrowser.messageManager;
  mm.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
}

registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
  info("finish() was called, cleaning up...");

  // Rollback any pref changes
  Object.keys(DEFAULT_PREFS).forEach(pref => {
    Preferences.set(pref, DEFAULT_PREFS[pref]);
  });

  Cu.forceGC();
});


function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished", false);
}

function addWindow(windowOptions) {
  let deferred = Promise.defer();
  let win = OpenBrowserWindow(windowOptions);

  whenDelayedStartupFinished(win, () => {
    executeSoon(() => {
      deferred.resolve(win);
    });
  });

  return deferred.promise;
}

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
  info(`Waiting for event: '${aEventName}' on ${aTarget}`);

  let deferred = Promise.defer();

  for (let [add, remove] of [
    ["on", "off"], // Use event emitter before DOM events for consistency
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"]
  ]) {
    if ((add in aTarget) && (remove in aTarget)) {
      aTarget[add](aEventName, function onEvent(...aArgs) {
        info(`Received event: '${aEventName}' on ${aTarget}`);
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

function initBackend(aUrl, targetOps={}) {
  info("Initializing a performance front.");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    // Attach addition options to `client`. This is used to force mock fronts
    // to smokescreen test different servers where memory or timeline actors
    // may not exist. Possible options that will actually work:
    // TEST_PERFORMANCE_LEGACY_FRONT = true
    // TEST_MOCK_TIMELINE_ACTOR = true
    // TEST_PROFILER_FILTER_STATUS = array
    merge(target, targetOps);

    let front = createPerformanceFront(target);
    yield front.connect();
    return { target, front };
  });
}

function initPerformance(aUrl, tool="performance", targetOps={}) {
  info("Initializing a performance pane.");

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    // Attach addition options to `client`. This is used to force mock fronts
    // to smokescreen test different servers where memory or timeline actors
    // may not exist. Possible options that will actually work:
    // TEST_PERFORMANCE_LEGACY_FRONT = true
    // TEST_MOCK_TIMELINE_ACTOR = true
    // TEST_PROFILER_FILTER_STATUS = array
    merge(target, targetOps);

    let toolbox = yield gDevTools.showToolbox(target, tool);

    // Wait for the performance tool to be spun up
    yield toolbox.initPerformance();

    // Panel is already initialized after `showToolbox` and `initPerformance`,
    // no need to wait for `open` here.
    let panel = toolbox.getCurrentPanel();
    return { target, panel, toolbox };
  });
}

/**
 * Initializes a webconsole panel. Returns a target, panel and toolbox reference.
 * Also returns a console property that allows calls to `profile` and `profileEnd`.
 */
function initConsole(aUrl, options) {
  return Task.spawn(function*() {
    let { target, toolbox, panel } = yield initPerformance(aUrl, "webconsole", options);
    let { hud } = panel;
    return {
      target, toolbox, panel, console: {
        profile: (s) => consoleExecute(hud, "profile", s),
        profileEnd: (s) => consoleExecute(hud, "profileEnd", s)
      }
    };
  });
}

function consoleExecute (console, method, val) {
  let { ui, jsterm } = console;
  let { promise, resolve } = Promise.defer();
  let message = `console.${method}("${val}")`;

  ui.on("new-messages", handler);
  jsterm.execute(message);

  let { console: c } = Cu.import("resource://gre/modules/devtools/shared/Console.jsm", {});
  function handler (event, messages) {
    for (let msg of messages) {
      if (msg.response._message === message) {
        ui.off("new-messages", handler);
        resolve();
        return;
      }
    }
  }
  return promise;
}

function* teardown(panel, win = window) {
  info("Destroying the performance tool.");

  let tab = panel.target.tab;
  yield panel._toolbox.destroy();
  yield removeTab(tab, win);
}

function idleWait(time) {
  return DevToolsUtils.waitForTime(time);
}

function busyWait(time) {
  let start = Date.now();
  let stack;
  while (Date.now() - start < time) { stack = Components.stack; }
}

function* consoleProfile(win, label) {
  let profileStart = once(win.PerformanceController, win.EVENTS.RECORDING_STARTED);
  consoleMethod("profile", label);
  yield profileStart;
}

function* consoleProfileEnd(win, label) {
  let ended = once(win.PerformanceController, win.EVENTS.RECORDING_STOPPED);
  consoleMethod("profileEnd", label);
  yield ended;
}

function command (button) {
  let ev = button.ownerDocument.createEvent("XULCommandEvent");
  ev.initCommandEvent("command", true, true, button.ownerDocument.defaultView, 0, false, false, false, false, null);
  button.dispatchEvent(ev);
}

function click (win, button) {
  EventUtils.sendMouseEvent({ type: "click" }, button, win);
}

function mousedown (win, button) {
  EventUtils.sendMouseEvent({ type: "mousedown" }, button, win);
}

function* startRecording(panel, options = {
  waitForOverview: true,
  waitForStateChanged: true
}) {
  let win = panel.panelWin;
  let clicked = panel.panelWin.PerformanceView.once(win.EVENTS.UI_START_RECORDING);
  let hasStarted = panel.panelWin.PerformanceController.once(win.EVENTS.RECORDING_STARTED);
  let button = win.$("#main-record-button");
  let stateChanged = options.waitForStateChanged
    ? once(win.PerformanceView, win.EVENTS.UI_STATE_CHANGED)
    : Promise.resolve();


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

  yield hasStarted;
  yield options.waitForOverview
    ? once(win.OverviewView, win.EVENTS.OVERVIEW_RENDERED)
    : Promise.resolve();

  yield stateChanged;

  is(win.PerformanceView.getState(), "recording",
    "The current state is 'recording'.");

  ok(button.hasAttribute("checked"),
    "The record button should still be checked.");
  ok(!button.hasAttribute("locked"),
    "The record button should not be locked.");
}

function* stopRecording(panel, options = {
  waitForOverview: true,
  waitForStateChanged: true
}) {
  let win = panel.panelWin;
  let clicked = panel.panelWin.PerformanceView.once(win.EVENTS.UI_STOP_RECORDING);
  let willStop = panel.panelWin.PerformanceController.once(win.EVENTS.RECORDING_WILL_STOP);
  let hasStopped = panel.panelWin.PerformanceController.once(win.EVENTS.RECORDING_STOPPED);
  let controllerStopped = panel.panelWin.PerformanceController.once(win.EVENTS.CONTROLLER_STOPPED_RECORDING);
  let button = win.$("#main-record-button");
  let overviewRendered = null;

  ok(button.hasAttribute("checked"),
    "The record button should already be checked.");
  ok(!button.hasAttribute("locked"),
    "The record button should not be locked yet.");

  click(win, button);
  yield clicked;

  yield willStop;
  ok(!button.hasAttribute("checked"),
    "The record button should not be checked.");
  ok(button.hasAttribute("locked"),
    "The record button should be locked.");

  let stateChanged = options.waitForStateChanged
    ? once(win.PerformanceView, win.EVENTS.UI_STATE_CHANGED)
    : Promise.resolve();

  yield hasStopped;

  // Wait for the final rendering of the overview, not a low res
  // incremental rendering and less likely to be from another rendering that was selected
  while (!overviewRendered && options.waitForOverview) {
    let [_, res] = yield onceSpread(win.OverviewView, win.EVENTS.OVERVIEW_RENDERED);
    if (res === win.FRAMERATE_GRAPH_HIGH_RES_INTERVAL) {
      overviewRendered = true;
    }
  }

  yield stateChanged;

  is(win.PerformanceView.getState(), "recorded",
    "The current state is 'recorded'.");

  ok(!button.hasAttribute("checked"),
    "The record button should not be checked.");
  ok(!button.hasAttribute("locked"),
    "The record button should not be locked.");

  yield controllerStopped;
}

function waitForWidgetsRendered(panel) {
  let {
    EVENTS,
    OverviewView,
    WaterfallView,
    JsCallTreeView,
    JsFlameGraphView,
    MemoryCallTreeView,
    MemoryFlameGraphView,
  } = panel.panelWin;

  return Promise.all([
    once(OverviewView, EVENTS.MARKERS_GRAPH_RENDERED),
    once(OverviewView, EVENTS.MEMORY_GRAPH_RENDERED),
    once(OverviewView, EVENTS.FRAMERATE_GRAPH_RENDERED),
    once(OverviewView, EVENTS.OVERVIEW_RENDERED),
    once(WaterfallView, EVENTS.WATERFALL_RENDERED),
    once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED),
    once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED),
    once(MemoryCallTreeView, EVENTS.MEMORY_CALL_TREE_RENDERED),
    once(MemoryFlameGraphView, EVENTS.MEMORY_FLAMEGRAPH_RENDERED),
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
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseDown({ testX: x, testY: y });
}

function dragStop(graph, x, y = 1) {
  x /= window.devicePixelRatio;
  y /= window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseUp({ testX: x, testY: y });
}

function dropSelection(graph) {
  graph.dropSelection();
  graph.emit("selecting");
}

/**
 * Fires a key event, like "VK_UP", "VK_DOWN", etc.
 */
function fireKey (e) {
  EventUtils.synthesizeKey(e, {});
}

function reload (aTarget, aEvent = "navigate") {
  aTarget.activeTab.reload();
  return once(aTarget, aEvent);
}

/**
* Forces cycle collection and GC, used in AudioNode destruction tests.
*/
function forceCC () {
  info("Triggering GC/CC...");
  SpecialPowers.DOMWindowUtils.cycleCollect();
  SpecialPowers.DOMWindowUtils.garbageCollect();
  SpecialPowers.DOMWindowUtils.garbageCollect();
}

/**
 * Inflate a particular sample's stack and return an array of strings.
 */
function getInflatedStackLocations(thread, sample) {
  let stackTable = thread.stackTable;
  let frameTable = thread.frameTable;
  let stringTable = thread.stringTable;
  let SAMPLE_STACK_SLOT = thread.samples.schema.stack;
  let STACK_PREFIX_SLOT = stackTable.schema.prefix;
  let STACK_FRAME_SLOT = stackTable.schema.frame;
  let FRAME_LOCATION_SLOT = frameTable.schema.location;

  // Build the stack from the raw data and accumulate the locations in
  // an array.
  let stackIndex = sample[SAMPLE_STACK_SLOT];
  let locations = [];
  while (stackIndex !== null) {
    let stackEntry = stackTable.data[stackIndex];
    let frame = frameTable.data[stackEntry[STACK_FRAME_SLOT]];
    locations.push(stringTable[frame[FRAME_LOCATION_SLOT]]);
    stackIndex = stackEntry[STACK_PREFIX_SLOT];
  }

  // The profiler tree is inverted, so reverse the array.
  return locations.reverse();
}

/**
 * Synthesize a profile for testing.
 */
function synthesizeProfileForTest(samples) {
  samples.unshift({
    time: 0,
    frames: [
      { location: "(root)" }
    ]
  });

  let uniqueStacks = new RecordingUtils.UniqueStacks();
  return RecordingUtils.deflateThread({
    samples: samples,
    markers: []
  }, uniqueStacks);
}

function isVisible (element) {
  return !element.classList.contains("hidden") && !element.hidden;
}

function within (actual, expected, fuzz, desc) {
  ok((actual - expected) <= fuzz, `${desc}: Expected ${actual} to be within ${fuzz} of ${expected}`);
}
