/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
var { console } = Cu.import("resource://gre/modules/Console.jsm", {});
var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});

var Services = require("Services");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var flags = require("devtools/shared/flags");
flags.testing = true;
flags.wantLogging = true;
flags.wantVerbose = false;

var { OS } = require("resource://gre/modules/osfile.jsm");
var { FileUtils } = require("resource://gre/modules/FileUtils.jsm");
var { TargetFactory } = require("devtools/client/framework/target");
var promise = require("promise");
var defer = require("devtools/shared/defer");
var { Task } = require("devtools/shared/task");
var { expectState } = require("devtools/server/actors/common");
var HeapSnapshotFileUtils = require("devtools/shared/heapsnapshot/HeapSnapshotFileUtils");
var HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");
var { addDebuggerToGlobal } = require("resource://gre/modules/jsdebugger.jsm");
var Store = require("devtools/client/memory/store");
var { L10N } = require("devtools/client/memory/utils");
var SYSTEM_PRINCIPAL =
  Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);

var EXPECTED_DTU_ASSERT_FAILURE_COUNT = 0;

do_register_cleanup(function () {
  equal(DevToolsUtils.assertionFailureCount, EXPECTED_DTU_ASSERT_FAILURE_COUNT,
        "Should have had the expected number of DevToolsUtils.assert() failures.");
});

function dumpn(msg) {
  dump(`MEMORY-TEST: ${msg}\n`);
}

function initDebugger() {
  let global = new Cu.Sandbox(SYSTEM_PRINCIPAL, { freshZone: true });
  addDebuggerToGlobal(global);
  return new global.Debugger();
}

function StubbedMemoryFront() {
  this.state = "detached";
  this.recordingAllocations = false;
  this.dbg = initDebugger();
}

StubbedMemoryFront.prototype.attach = Task.async(function* () {
  this.state = "attached";
});

StubbedMemoryFront.prototype.detach = Task.async(function* () {
  this.state = "detached";
});

StubbedMemoryFront.prototype.saveHeapSnapshot = expectState("attached",
  Task.async(function* () {
    return ThreadSafeChromeUtils.saveHeapSnapshot({ runtime: true });
  }), "saveHeapSnapshot");

StubbedMemoryFront.prototype.startRecordingAllocations = expectState("attached",
  Task.async(function* () {
    this.recordingAllocations = true;
  }));

StubbedMemoryFront.prototype.stopRecordingAllocations = expectState("attached",
  Task.async(function* () {
    this.recordingAllocations = false;
  }));

function waitUntilSnapshotState(store, expected) {
  let predicate = () => {
    let snapshots = store.getState().snapshots;
    do_print(snapshots.map(x => x.state));
    return snapshots.length === expected.length &&
           expected.every((state, i) => state === "*" || snapshots[i].state === state);
  };
  do_print(`Waiting for snapshots to be of state: ${expected}`);
  return waitUntilState(store, predicate);
}

function findReportLeafIndex(node, name = null) {
  if (node.reportLeafIndex && (!name || node.name === name)) {
    return node.reportLeafIndex;
  }

  if (node.children) {
    for (let child of node.children) {
      const found = findReportLeafIndex(child);
      if (found) {
        return found;
      }
    }
  }

  return null;
}

function waitUntilCensusState(store, getCensus, expected) {
  let predicate = () => {
    let snapshots = store.getState().snapshots;

    do_print("Current census state:" +
             snapshots.map(x => getCensus(x) ? getCensus(x).state : null));

    return snapshots.length === expected.length &&
           expected.every((state, i) => {
             let census = getCensus(snapshots[i]);
             return (state === "*") ||
                    (!census && !state) ||
                    (census && census.state === state);
           });
  };
  do_print(`Waiting for snapshots' censuses to be of state: ${expected}`);
  return waitUntilState(store, predicate);
}

function* createTempFile() {
  let file = FileUtils.getFile("TmpD", ["tmp.fxsnapshot"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  let destPath = file.path;
  let stat = yield OS.File.stat(destPath);
  ok(stat.size === 0, "new file is 0 bytes at start");
  return destPath;
}
