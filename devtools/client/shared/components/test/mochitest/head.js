/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://testing-common/Assert.jsm");
Cu.import("resource://gre/modules/Task.jsm");
var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Disable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
var gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", false);

// Enable the memory tool for all tests.
var gMemoryToolEnabled = Services.prefs.getBoolPref("devtools.memory.enabled");
Services.prefs.setBoolPref("devtools.memory.enabled", true);

var { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
var { require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
var { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
var { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
var { DebuggerClient } = Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { BrowserLoader } = Cu.import("resource://devtools/client/shared/browser-loader.js", {});
var { TargetFactory } = require("devtools/client/framework/target");
var { Toolbox } = require("devtools/client/framework/toolbox");

DevToolsUtils.testing = true;
var { require: browserRequire } = BrowserLoader("resource://devtools/client/shared/", this);

var EXAMPLE_URL = "http://example.com/browser/browser/devtools/shared/test/";

// Encoding of the following tree/forest:
//
// A
// |-- B
// |   |-- E
// |   |   |-- K
// |   |   `-- L
// |   |-- F
// |   `-- G
// |-- C
// |   |-- H
// |   `-- I
// `-- D
//     `-- J
// M
// `-- N
//     `-- O
var TEST_TREE = {
  children: {
    A: ["B", "C", "D"],
    B: ["E", "F", "G"],
    C: ["H", "I"],
    D: ["J"],
    E: ["K", "L"],
    F: [],
    G: [],
    H: [],
    I: [],
    J: [],
    K: [],
    L: [],
    M: ["N"],
    N: ["O"],
    O: []
  },
  parent: {
    A: null,
    B: "A",
    C: "A",
    D: "A",
    E: "B",
    F: "B",
    G: "B",
    H: "C",
    I: "C",
    J: "D",
    K: "E",
    L: "E",
    M: null,
    N: "M",
    O: "N"
  }
};

var TEST_TREE_INTERFACE = {
  getParent: x => TEST_TREE.parent[x],
  getChildren: x => TEST_TREE.children[x],
  renderItem: (x, depth, focused, arrow) => "-".repeat(depth) + x + ":" + focused + "\n",
  getRoots: () => ["A", "M"],
  getKey: x => "key-" + x,
  itemHeight: 1
};

// All tests are asynchronous.
SimpleTest.waitForExplicitFinish();

SimpleTest.registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.memory.enabled", gMemoryToolEnabled);
});

function addTab(url) {
  info("Adding tab: " + url);

  var deferred = promise.defer();
  var tab = gBrowser.selectedTab = gBrowser.addTab(url);
  var linkedBrowser = tab.linkedBrowser;

  linkedBrowser.addEventListener("load", function onLoad() {
    linkedBrowser.removeEventListener("load", onLoad, true);
    info("Tab added and finished loading: " + url);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(tab) {
  info("Removing tab.");

  var deferred = promise.defer();
  var tabContainer = gBrowser.tabContainer;

  tabContainer.addEventListener("TabClose", function onClose(aEvent) {
    tabContainer.removeEventListener("TabClose", onClose, false);
    info("Tab removed and finished closing.");
    deferred.resolve();
  }, false);

  gBrowser.removeTab(tab);
  return deferred.promise;
}

var withTab = Task.async(function* (url, generator) {
  var tab = yield addTab(url);
  try {
    yield* generator(tab);
  } finally {
    yield removeTab(tab);
  }
});

var openMemoryTool = Task.async(function* (tab) {
  info("Initializing a memory panel.");

  var target = TargetFactory.forTab(tab);
  var debuggee = target.window.wrappedJSObject;

  yield target.makeRemote();

  var toolbox = yield gDevTools.showToolbox(target, "memory");
  var panel = toolbox.getCurrentPanel();
  return [target, debuggee, panel];
});

var closeMemoryTool = Task.async(function* (panel) {
  info("Closing a memory panel");
  yield panel._toolbox.destroy();
});

var withMemoryTool = Task.async(function* (tab, generator) {
  var [target, debuggee, panel] = yield openMemoryTool(tab);
  try {
    yield* generator(target, debuggee, panel);
  } finally {
    yield closeMemoryTool(panel);
  }
});

var withTabAndMemoryTool = Task.async(function* (url, generator) {
  yield withTab(url, function* (tab) {
    yield withMemoryTool(tab, function* (target, debuggee, panel) {
      yield* generator(tab, target, debuggee, panel);
    });
  });
});

function reload(target) {
  info("Reloading tab.");
  var deferred = promise.defer();
  target.once("navigate", deferred.resolve);
  target.activeTab.reload();
  return deferred.promise;
}

function onNextAnimationFrame(fn) {
  return () =>
    requestAnimationFrame(() =>
      requestAnimationFrame(fn));
}

function setState(component, newState) {
  var deferred = promise.defer();
  component.setState(newState, onNextAnimationFrame(deferred.resolve));
  return deferred.promise;
}

function setProps(component, newState) {
  var deferred = promise.defer();
  component.setProps(newState, onNextAnimationFrame(deferred.resolve));
  return deferred.promise;
}

function dumpn(msg) {
  dump(`MEMORY-TEST: ${msg}\n`);
}

function isRenderedTree(actual, expectedDescription, msg) {
  const expected = expectedDescription.map(x => x + "\n").join("");
  dumpn(`Expected tree:\n${expected}`);
  dumpn(`Actual tree:\n${actual}`);
  is(actual, expected, msg);
}
