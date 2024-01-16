/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const wdm = Cc["@mozilla.org/dom/workers/workerdebuggermanager;1"].getService(
  Ci.nsIWorkerDebuggerManager
);

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

function generateDebuggers() {
  return wdm.getWorkerDebuggerEnumerator();
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
      onRegister(dbg) {
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
      },
    });
  });
}

function waitForUnregister(url) {
  return new Promise(function (resolve) {
    wdm.addListener({
      onUnregister(dbg) {
        if (dbg.url !== url) {
          return;
        }
        ok(true, "Debugger with url " + url + " should be unregistered.");
        wdm.removeListener(this);
        resolve();
      },
    });
  });
}

function waitForDebuggerClose(dbg) {
  return new Promise(function (resolve) {
    dbg.addListener({
      onClose() {
        ok(true, "Debugger should be closed.");
        dbg.removeListener(this);
        resolve();
      },
    });
  });
}

function waitForDebuggerError(dbg) {
  return new Promise(function (resolve) {
    dbg.addListener({
      onError(filename, lineno, message) {
        dbg.removeListener(this);
        resolve(new Error(message, filename, lineno));
      },
    });
  });
}

function waitForDebuggerMessage(dbg, message) {
  return new Promise(function (resolve) {
    dbg.addListener({
      onMessage(message1) {
        if (message !== message1) {
          return;
        }
        ok(true, "Should receive " + message + " message from debugger.");
        dbg.removeListener(this);
        resolve();
      },
    });
  });
}

function waitForWindowMessage(window, message) {
  return new Promise(function (resolve) {
    let onmessage = function (event) {
      // eslint-disable-next-line no-self-compare
      if (event.data !== event.data) {
        return;
      }
      window.removeEventListener("message", onmessage);
      resolve();
    };
    window.addEventListener("message", onmessage);
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
  // There used to be old logic which expects promises to be resolved in
  // succession, but where it seems like this was an incorrect assumption.
  // Assuming this change sticks, bug 1861778 tracks removing this method
  // entirely in favor of Promise.all at the call-sites or transform the callers
  // into explicitly documented awaited sequences.
  return Promise.all(promises);
}
