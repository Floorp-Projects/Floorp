/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
var { gDevTools } = Cu.import("resource://devtools/client/framework/gDevTools.jsm", {});
var { console } = Cu.import("resource://gre/modules/Console.jsm", {});
var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});

var DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.testing = true;
DevToolsUtils.dumpn.wantLogging = true;
DevToolsUtils.dumpv.wantLogging = true;

var { OS } = require("resource://gre/modules/osfile.jsm");
var { FileUtils } = require("resource://gre/modules/FileUtils.jsm");
var { TargetFactory } = require("devtools/client/framework/target");
var promise = require("promise");
var { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
var { expectState } = require("devtools/server/actors/common");
var HeapSnapshotFileUtils = require("devtools/shared/heapsnapshot/HeapSnapshotFileUtils");
var HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");
var { addDebuggerToGlobal } = require("resource://gre/modules/jsdebugger.jsm");
var Store = require("devtools/client/memory/store");
var SYSTEM_PRINCIPAL = Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);

function dumpn(msg) {
  dump(`MEMORY-TEST: ${msg}\n`);
}

function initDebugger () {
  let global = new Cu.Sandbox(SYSTEM_PRINCIPAL, { freshZone: true });
  addDebuggerToGlobal(global);
  return new global.Debugger();
}

function StubbedMemoryFront () {
  this.state = "detached";
  this.recordingAllocations = false;
  this.dbg = initDebugger();
}

StubbedMemoryFront.prototype.attach = Task.async(function *() {
  this.state = "attached";
});

StubbedMemoryFront.prototype.detach = Task.async(function *() {
  this.state = "detached";
});

StubbedMemoryFront.prototype.saveHeapSnapshot = expectState("attached", Task.async(function *() {
  return ThreadSafeChromeUtils.saveHeapSnapshot({ runtime: true });
}), "saveHeapSnapshot");

StubbedMemoryFront.prototype.startRecordingAllocations = expectState("attached", Task.async(function* () {
  this.recordingAllocations = true;
}));

StubbedMemoryFront.prototype.stopRecordingAllocations = expectState("attached", Task.async(function* () {
  this.recordingAllocations = false;
}));

function waitUntilState (store, predicate) {
  let deferred = promise.defer();
  let unsubscribe = store.subscribe(check);

  function check () {
    if (predicate(store.getState())) {
      unsubscribe();
      deferred.resolve()
    }
  }

  // Fire the check immediately incase the action has already occurred
  check();

  return deferred.promise;
}

function waitUntilAction (store, actionType) {
  let deferred = promise.defer();
  let unsubscribe = store.subscribe(check);
  let history = store.history;
  let index = history.length;

  do_print(`Waiting for action "${actionType}"`);
  function check () {
    let action = history[index++];
    if (action && action.type === actionType) {
      do_print(`Found action "${actionType}"`);
      unsubscribe();
      deferred.resolve(store.getState());
    }
  }

  return deferred.promise;
}

function waitUntilSnapshotState (store, expected) {
  let predicate = () => {
    let snapshots = store.getState().snapshots;
    do_print(snapshots.map(x => x.state));
    return snapshots.length === expected.length &&
           expected.every((state, i) => state === "*" || snapshots[i].state === state);
  };
  do_print(`Waiting for snapshots to be of state: ${expected}`);
  return waitUntilState(store, predicate);
}

function isBreakdownType (report, type) {
  // Little sanity check, all reports should have at least a children array.
  if (!report || !Array.isArray(report.children)) {
    return false;
  }
  switch (type) {
    case "coarseType":
      return report.children.find(c => c.name === "objects");
    case "objectClass":
      return report.children.find(c => c.name === "Function");
    case "internalType":
      return report.children.find(c => c.name === "js::BaseShape") &&
             !report.children.find(c => c.name === "objects");
    default:
      throw new Error(`isBreakdownType does not yet support ${type}`);
  }
}

function *createTempFile () {
  let file = FileUtils.getFile("TmpD", ["tmp.fxsnapshot"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  let destPath = file.path;
  let stat = yield OS.File.stat(destPath);
  ok(stat.size === 0, "new file is 0 bytes at start");
  return destPath;
}
