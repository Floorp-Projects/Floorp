/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// The path to the top level directory.
const depth = "../../../../";

var testGenerator;
var testHarnessGenerator;
var workerScriptPaths = [];

loadScript("dom/quota/test/common/mochitest.js");

function loadScript(path) {
  const url = new URL(depth + path, window.location.href);
  SpecialPowers.Services.scriptloader.loadSubScript(url.href, this);
}

function loadWorkerScript(path) {
  const url = new URL(depth + path, window.location.href);
  workerScriptPaths.push(url.href);
}

function* testHarnessSteps() {
  function nextTestHarnessStep(val) {
    testHarnessGenerator.next(val);
  }

  info("Enabling storage testing");

  enableStorageTesting().then(nextTestHarnessStep);
  yield undefined;

  info("Pushing preferences");

  SpecialPowers.pushPrefEnv(
    {
      set: [["dom.storageManager.prompt.testing", true]],
    },
    nextTestHarnessStep
  );
  yield undefined;

  info("Clearing old databases");

  clearAllDatabases(nextTestHarnessStep);
  yield undefined;

  info("Running test in given scopes");

  if (workerScriptPaths.length) {
    info("Running test in a worker");

    let workerScriptBlob = new Blob(["(" + workerScript.toString() + ")();"], {
      type: "text/javascript",
    });
    let workerScriptURL = URL.createObjectURL(workerScriptBlob);

    let worker = new Worker(workerScriptURL);

    worker.onerror = function(event) {
      ok(false, "Worker had an error: " + event.message);
      worker.terminate();
      nextTestHarnessStep();
    };

    worker.onmessage = function(event) {
      let message = event.data;
      switch (message.op) {
        case "ok":
          ok(message.condition, message.name + " - " + message.diag);
          break;

        case "todo":
          todo(message.condition, message.name, message.diag);
          break;

        case "info":
          info(message.msg);
          break;

        case "ready":
          worker.postMessage({ op: "load", files: workerScriptPaths });
          break;

        case "loaded":
          worker.postMessage({ op: "start" });
          break;

        case "done":
          ok(true, "Worker finished");
          nextTestHarnessStep();
          break;

        case "clearAllDatabases":
          clearAllDatabases(function() {
            worker.postMessage({ op: "clearAllDatabasesDone" });
          });
          break;

        default:
          ok(
            false,
            "Received a bad message from worker: " + JSON.stringify(message)
          );
          nextTestHarnessStep();
      }
    };

    URL.revokeObjectURL(workerScriptURL);

    yield undefined;

    worker.terminate();
    worker = null;

    clearAllDatabases(nextTestHarnessStep);
    yield undefined;
  }

  info("Running test in main thread");

  // Now run the test script in the main thread.
  if (testSteps.constructor.name === "AsyncFunction") {
    SimpleTest.registerCleanupFunction(async function() {
      await requestFinished(clearAllDatabases());
    });

    add_task(testSteps);
  } else {
    testGenerator = testSteps();
    testGenerator.next();

    yield undefined;
  }
}

if (!window.runTest) {
  window.runTest = function() {
    SimpleTest.waitForExplicitFinish();
    testHarnessGenerator = testHarnessSteps();
    testHarnessGenerator.next();
  };
}

function finishTest() {
  SimpleTest.executeSoon(function() {
    clearAllDatabases(function() {
      SimpleTest.finish();
    });
  });
}

function grabArgAndContinueHandler(arg) {
  testGenerator.next(arg);
}

function continueToNextStep() {
  SimpleTest.executeSoon(function() {
    testGenerator.next();
  });
}

function continueToNextStepSync() {
  testGenerator.next();
}

function workerScript() {
  "use strict";

  self.testGenerator = null;

  self.repr = function(_thing_) {
    if (typeof _thing_ == "undefined") {
      return "undefined";
    }

    let str;

    try {
      str = _thing_ + "";
    } catch (e) {
      return "[" + typeof _thing_ + "]";
    }

    if (typeof _thing_ == "function") {
      str = str.replace(/^\s+/, "");
      let idx = str.indexOf("{");
      if (idx != -1) {
        str = str.substr(0, idx) + "{...}";
      }
    }

    return str;
  };

  self.ok = function(_condition_, _name_, _diag_) {
    self.postMessage({
      op: "ok",
      condition: !!_condition_,
      name: _name_,
      diag: _diag_,
    });
  };

  self.is = function(_a_, _b_, _name_) {
    let pass = _a_ == _b_;
    let diag = pass ? "" : "got " + repr(_a_) + ", expected " + repr(_b_);
    ok(pass, _name_, diag);
  };

  self.isnot = function(_a_, _b_, _name_) {
    let pass = _a_ != _b_;
    let diag = pass ? "" : "didn't expect " + repr(_a_) + ", but got it";
    ok(pass, _name_, diag);
  };

  self.todo = function(_condition_, _name_, _diag_) {
    self.postMessage({
      op: "todo",
      condition: !!_condition_,
      name: _name_,
      diag: _diag_,
    });
  };

  self.info = function(_msg_) {
    self.postMessage({ op: "info", msg: _msg_ });
  };

  self.executeSoon = function(_fun_) {
    var channel = new MessageChannel();
    channel.port1.postMessage("");
    channel.port2.onmessage = function(event) {
      _fun_();
    };
  };

  self.finishTest = function() {
    self.postMessage({ op: "done" });
  };

  self.grabArgAndContinueHandler = function(_arg_) {
    testGenerator.next(_arg_);
  };

  self.continueToNextStep = function() {
    executeSoon(function() {
      testGenerator.next();
    });
  };

  self.continueToNextStepSync = function() {
    testGenerator.next();
  };

  self._clearAllDatabasesCallback = undefined;
  self.clearAllDatabases = function(_callback_) {
    self._clearAllDatabasesCallback = _callback_;
    self.postMessage({ op: "clearAllDatabases" });
  };

  self.onerror = function(_message_, _file_, _line_) {
    ok(
      false,
      "Worker: uncaught exception [" +
        _file_ +
        ":" +
        _line_ +
        "]: '" +
        _message_ +
        "'"
    );
    self.finishTest();
    self.close();
    return true;
  };

  self.onmessage = function(_event_) {
    let message = _event_.data;
    switch (message.op) {
      case "load":
        info("Worker: loading " + JSON.stringify(message.files));
        self.importScripts(message.files);
        self.postMessage({ op: "loaded" });
        break;

      case "start":
        executeSoon(function() {
          info("Worker: starting tests");
          testGenerator = testSteps();
          testGenerator.next();
        });
        break;

      case "clearAllDatabasesDone":
        info("Worker: all databases are cleared");
        if (self._clearAllDatabasesCallback) {
          self._clearAllDatabasesCallback();
        }
        break;

      default:
        throw new Error(
          "Received a bad message from parent: " + JSON.stringify(message)
        );
    }
  };

  self.postMessage({ op: "ready" });
}
