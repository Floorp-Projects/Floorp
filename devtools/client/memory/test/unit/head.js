/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
var { gDevTools } = Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm", {});
var { console } = Cu.import("resource://gre/modules/Console.jsm", {});
var { require } = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
var { TargetFactory } = require("devtools/client/framework/target");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var promise = require("promise");
var { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
var { expectState } = require("devtools/server/actors/common");
var HeapSnapshotFileUtils = require("devtools/shared/heapsnapshot/HeapSnapshotFileUtils");
var HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");
var { addDebuggerToGlobal } = require("resource://gre/modules/jsdebugger.jsm");
var Store = require("devtools/client/memory/store");
var SYSTEM_PRINCIPAL = Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);

DevToolsUtils.testing = true;

function initDebugger () {
  let global = new Cu.Sandbox(SYSTEM_PRINCIPAL, { freshZone: true });
  addDebuggerToGlobal(global);
  return new global.Debugger();
}

function StubbedMemoryFront () {
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

function isBreakdownType (census, type) {
  // Little sanity check, all censuses should have atleast a children array
  if (!census || !Array.isArray(census.children)) {
    return false;
  }
  switch (type) {
    case "coarseType":
      return census.children.find(c => c.name === "objects");
    case "objectClass":
      return census.children.find(c => c.name === "Function");
    case "internalType":
      return census.children.find(c => c.name === "js::BaseShape") &&
             !census.children.find(c => c.name === "objects");
    default:
      throw new Error(`isBreakdownType does not yet support ${type}`);
  }
}
