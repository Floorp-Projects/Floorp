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
  let path = ThreadSafeChromeUtils.saveHeapSnapshot({ debugger: this.dbg });
  return HeapSnapshotFileUtils.getSnapshotIdFromPath(path);
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
