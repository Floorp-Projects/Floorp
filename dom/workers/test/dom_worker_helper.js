/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const wdm = Cc["@mozilla.org/dom/workers/workerdebuggermanager;1"].
            getService(Ci.nsIWorkerDebuggerManager);

const BASE_URL = "chrome://mochitests/content/chrome/dom/workers/test/";

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

function findDebugger(url) {
  for (let dbg of generateDebuggers()) {
    if (dbg.url === url) {
      return dbg;
    }
  }
  return null;
}

function waitForRegister(url, dbgUrl) {
  return new Promise(function (resolve) {
    wdm.addListener({
      onRegister: function (dbg) {
        if (dbg.url !== url) {
          return;
        }
        ok(true, "Debugger with url " + url + " should be registered.");
        wdm.removeListener(this);
        if (dbgUrl) {
          info("Initializing worker debugger with url " + url + ".");
          dbg.initialize(dbgUrl);
        }
        resolve(dbg);
      }
    });
  });
}

function waitForUnregister(url) {
  return new Promise(function (resolve) {
    wdm.addListener({
      onUnregister: function (dbg) {
        if (dbg.url !== url) {
          return;
        }
        ok(true, "Debugger with url " + url + " should be unregistered.");
        wdm.removeListener(this);
        resolve();
      }
    });
  });
}

function waitForDebuggerClose(dbg) {
  return new Promise(function (resolve) {
    dbg.addListener({
      onClose: function () {
        ok(true, "Debugger should be closed.");
        dbg.removeListener(this);
        resolve();
      }
    });
  });
}

function waitForDebuggerError(dbg) {
  return new Promise(function (resolve) {
    dbg.addListener({
      onError: function (filename, lineno, message) {
        dbg.removeListener(this);
        resolve(new Error(message, filename, lineno));
      }
    });
  });
}

function waitForDebuggerMessage(dbg, message) {
  return new Promise(function (resolve) {
    dbg.addListener({
      onMessage: function (message1) {
        if (message !== message1) {
          return;
        }
        ok(true, "Should receive " + message + " message from debugger.");
        dbg.removeListener(this);
        resolve();
      }
    });
  });
}

function waitForDebuggerFreeze(dbg) {
  return new Promise(function (resolve) {
    dbg.addListener({
      onFreeze: function () {
        dbg.removeListener(this);
        resolve();
      }
    });
  });
}

function waitForDebuggerThaw(dbg) {
  return new Promise(function (resolve) {
    dbg.addListener({
      onThaw: function () {
        dbg.removeListener(this);
        resolve();
      }
    });
  });
}

function waitForWindowMessage(window, message) {
  return new Promise(function (resolve) {
    let onmessage = function (event) {
      if (event.data !== event.data) {
        return;
      }
      window.removeEventListener("message", onmessage, false);
      resolve();
    };
    window.addEventListener("message", onmessage, false);
  });
}

function waitForWorkerMessage(worker, message) {
  return new Promise(function (resolve) {
    worker.addEventListener("message", function onmessage(event) {
      if (event.data !== message) {
        return;
      }
      ok(true, "Should receive " + message + " message from worker.");
      worker.removeEventListener("message", onmessage);
      resolve();
    });
  });
}

function waitForMultiple(promises) {
  return new Promise(function (resolve) {
    let values = [];
    for (let i = 0; i < promises.length; ++i) {
      let index = i;
      promises[i].then(function (value) {
        is(index + 1, values.length + 1,
           "Promise " + (values.length + 1) + " out of " + promises.length +
           " should be resolved.");
        values.push(value);
        if (values.length === promises.length) {
          resolve(values);
        }
      });
    }
  });
};
