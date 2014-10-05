/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Disable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
let gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", false);

let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
let { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});
let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { DebuggerClient } = Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});

let nsIProfilerModule = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
let TargetFactory = devtools.TargetFactory;
let Toolbox = devtools.Toolbox;

const EXAMPLE_URL = "http://example.com/browser/browser/devtools/profiler/test/";
const SIMPLE_URL = EXAMPLE_URL + "doc_simple-test.html";

// All tests are asynchronous.
waitForExplicitFinish();

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);

  // Make sure the profiler module is stopped when the test finishes.
  nsIProfilerModule.StopProfiler();
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

function* initFrontend(url, id = "jsprofiler") {
  info("Initializing a " + id + " pane.");

  let tab = yield addTab(url);
  let target = TargetFactory.forTab(tab);
  let debuggee = target.window.wrappedJSObject;

  yield target.makeRemote();

  let toolbox = yield gDevTools.showToolbox(target, id);
  let panel = toolbox.getCurrentPanel();
  return [target, debuggee, panel];
}

function* teardown(panel) {
  info("Destroying the specified profiler.");

  let tab = panel.target.tab;
  yield panel._toolbox.destroy();
  yield removeTab(tab);
}

function* waitForProfilerConnection() {
  let profilerConnected = promise.defer();
  let profilerConnectionObserver = () => profilerConnected.resolve();
  Services.obs.addObserver(profilerConnectionObserver, "profiler-connection-opened", false);

  yield profilerConnected.promise;
  Services.obs.removeObserver(profilerConnectionObserver, "profiler-connection-opened");
}

function busyWait(time) {
  let start = Date.now();
  let stack;
  while (Date.now() - start < time) { stack = Components.stack; }
}

function idleWait(time) {
  return DevToolsUtils.waitForTime(time);
}

function* startRecording(panel) {
  let win = panel.panelWin;
  let started = win.once(win.EVENTS.RECORDING_STARTED);
  let button = win.$("#record-button");

  ok(!button.hasAttribute("checked"),
    "The record button should not be checked yet.");
  ok(!button.hasAttribute("locked"),
    "The record button should not be locked yet.");

  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  yield started;

  ok(button.hasAttribute("checked"),
    "The record button should is now checked.");
  ok(!button.hasAttribute("locked"),
    "The record button should still not be locked.");
}

function* stopRecording(panel, { waitForDisplay }) {
  let win = panel.panelWin;
  let ended = win.once(win.EVENTS.RECORDING_ENDED);
  let displayed = win.once(win.EVENTS.RECORDING_DISPLAYED);
  let button = win.$("#record-button");

  ok(button.hasAttribute("checked"),
    "The record button should already be checked.");
  ok(!button.hasAttribute("locked"),
    "The record button should not be locked.");

  EventUtils.synthesizeMouseAtCenter(button, {}, win);
  yield ended;

  ok(button.hasAttribute("checked"),
    "The record button should still be checked.");
  ok(button.hasAttribute("locked"),
    "The record button now has a locked attribute.");

  if (waitForDisplay) {
    yield displayed;
    if (!win.RecordingsListView.getItemForPredicate(e => e.isRecording)) {
      ok(!button.hasAttribute("checked"),
        "The record button should not be checked anymore.");
    }
    ok(!button.hasAttribute("locked"),
      "The record button should not be locked anymore.");
  }
}
