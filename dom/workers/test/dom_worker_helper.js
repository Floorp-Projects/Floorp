/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const wdm = Cc["@mozilla.org/dom/workers/workerdebuggermanager;1"].
            getService(Ci.nsIWorkerDebuggerManager);

var gRemainingTests = 0;

function waitForWorkerFinish() {
  if (gRemainingTests == 0) {
    SimpleTest.waitForExplicitFinish();
  }
  ++gRemainingTests;
}

function finish() {
  --gRemainingTests;
  if (gRemainingTests == 0) {
    SimpleTest.finish();
  }
}

function assertThrows(fun, message) {
  let throws = false;
  try {
    fun();
  } catch (e) {
    throws = true;
  }
  ok(throws, message);
}

function* generateDebuggers() {
  let e = wdm.getWorkerDebuggerEnumerator();
  while (e.hasMoreElements()) {
    let dbg = e.getNext().QueryInterface(Ci.nsIWorkerDebugger);
    yield dbg;
  }
}

function findDebugger(predicate) {
  for (let dbg of generateDebuggers()) {
    if (predicate(dbg)) {
      return dbg;
    }
  }
  return null;
}

function waitForRegister(predicate = () => true) {
  return new Promise(function (resolve) {
    wdm.addListener({
      onRegister: function (dbg) {
        if (!predicate(dbg)) {
          return;
        }
        wdm.removeListener(this);
        resolve(dbg);
      }
    });
  });
}

function waitForUnregister(predicate = () => true) {
  return new Promise(function (resolve) {
    wdm.addListener({
      onUnregister: function (dbg) {
        if (!predicate(dbg)) {
          return;
        }
        wdm.removeListener(this);
        resolve(dbg);
      }
    });
  });
}

function waitForMultiple(promises) {
  return new Promise(function (resolve) {
    let results = [];
    for (let i = 0; i < promises.length; ++i) {
      let promise = promises[i];
      let index = i;
      promise.then(function (result) {
        is(results.length, index, "events should occur in the specified order");
        results.push(result);
        if (results.length === promises.length) {
          resolve(results);
        }
      });
    }
  });
};
