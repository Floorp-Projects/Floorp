/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// via xpcshell.ini
/* import-globals-from ../../../shared/test/shared-head.js */

Services.prefs.setBoolPref("devtools.testing", true);
Services.prefs.setBoolPref("devtools.debugger.log", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.testing");
  Services.prefs.clearUserPref("devtools.debugger.log");
});

var { FileUtils } = require("resource://gre/modules/FileUtils.jsm");
var { expectState } = require("devtools/server/actors/common");
var HeapSnapshotFileUtils = require("devtools/shared/heapsnapshot/HeapSnapshotFileUtils");
var HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");
var { addDebuggerToGlobal } = require("resource://gre/modules/jsdebugger.jsm");
var Store = require("devtools/client/memory/store");
var { L10N } = require("devtools/client/memory/utils");
var SYSTEM_PRINCIPAL = Cc["@mozilla.org/systemprincipal;1"].createInstance(
  Ci.nsIPrincipal
);

var EXPECTED_DTU_ASSERT_FAILURE_COUNT = 0;

registerCleanupFunction(function() {
  equal(
    DevToolsUtils.assertionFailureCount,
    EXPECTED_DTU_ASSERT_FAILURE_COUNT,
    "Should have had the expected number of DevToolsUtils.assert() failures."
  );
});

function dumpn(msg) {
  dump(`MEMORY-TEST: ${msg}\n`);
}

function initDebugger() {
  const global = new Cu.Sandbox(SYSTEM_PRINCIPAL, { freshZone: true });
  addDebuggerToGlobal(global);
  return new global.Debugger();
}

function StubbedMemoryFront() {
  this.state = "detached";
  this.dbg = initDebugger();
}

StubbedMemoryFront.prototype.attach = async function() {
  this.state = "attached";
};

StubbedMemoryFront.prototype.detach = async function() {
  this.state = "detached";
};

StubbedMemoryFront.prototype.saveHeapSnapshot = expectState(
  "attached",
  async function() {
    return ChromeUtils.saveHeapSnapshot({ runtime: true });
  },
  "saveHeapSnapshot"
);

StubbedMemoryFront.prototype.startRecordingAllocations = expectState(
  "attached",
  async function() {}
);

StubbedMemoryFront.prototype.stopRecordingAllocations = expectState(
  "attached",
  async function() {}
);

function waitUntilSnapshotState(store, expected) {
  const predicate = () => {
    const snapshots = store.getState().snapshots;
    info(snapshots.map(x => x.state));
    return (
      snapshots.length === expected.length &&
      expected.every(
        (state, i) => state === "*" || snapshots[i].state === state
      )
    );
  };
  info(`Waiting for snapshots to be of state: ${expected}`);
  return waitUntilState(store, predicate);
}

function findReportLeafIndex(node, name = null) {
  if (node.reportLeafIndex && (!name || node.name === name)) {
    return node.reportLeafIndex;
  }

  if (node.children) {
    for (const child of node.children) {
      const found = findReportLeafIndex(child);
      if (found) {
        return found;
      }
    }
  }

  return null;
}

function waitUntilCensusState(store, getCensus, expected) {
  const predicate = () => {
    const snapshots = store.getState().snapshots;

    info(
      "Current census state:" +
        snapshots.map(x => (getCensus(x) ? getCensus(x).state : null))
    );

    return (
      snapshots.length === expected.length &&
      expected.every((state, i) => {
        const census = getCensus(snapshots[i]);
        return (
          state === "*" ||
          (!census && !state) ||
          (census && census.state === state)
        );
      })
    );
  };
  info(`Waiting for snapshots' censuses to be of state: ${expected}`);
  return waitUntilState(store, predicate);
}

async function createTempFile() {
  const file = FileUtils.getFile("TmpD", ["tmp.fxsnapshot"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
  const destPath = file.path;
  const stat = await IOUtils.stat(destPath);
  ok(stat.size === 0, "new file is 0 bytes at start");
  return destPath;
}

// This is a copy of the same method from shared-head.js as
// xpcshell test aren't using shared-head.js
/**
 * Wait for a specific action type to be dispatched.
 *
 * If the action is async and defines a `status` property, this helper will wait
 * for the status to reach either "error" or "done".
 *
 * @param {Object} store
 *        Redux store where the action should be dispatched.
 * @param {String} actionType
 *        The actionType to wait for.
 * @param {Number} repeat
 *        Optional, number of time the action is expected to be dispatched.
 *        Defaults to 1
 * @return {Promise}
 */
function waitForDispatch(store, actionType, repeat = 1) {
  let count = 0;
  return new Promise(resolve => {
    store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => {
        const isDone =
          !action.status ||
          action.status === "done" ||
          action.status === "error";

        if (action.type === actionType && isDone && ++count == repeat) {
          return true;
        }

        return false;
      },
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}
