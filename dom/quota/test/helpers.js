/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function clearAllDatabases(callback)
{
  let qms = SpecialPowers.Services.qms;
  let principal = SpecialPowers.wrap(document).nodePrincipal;
  let request = qms.clearStoragesForPrincipal(principal);
  let cb = SpecialPowers.wrapCallback(callback);
  request.callback = cb;
}

var testHarnessGenerator = testHarnessSteps();
testHarnessGenerator.next();

function* testHarnessSteps()
{
  function nextTestHarnessStep(val)
  {
    testHarnessGenerator.next(val);
  }

  let testScriptPath;
  let testScriptFilename;

  let scripts = document.getElementsByTagName("script");
  for (let i = 0; i < scripts.length; i++) {
    let src = scripts[i].src;
    let match = src.match(/quota\/test\/unit\/(test_[^\/]+\.js)$/);
    if (match && match.length == 2) {
      testScriptPath = src;
      testScriptFilename = match[1];
      break;
    }
  }

  yield undefined;

  info("Pushing preferences");

  SpecialPowers.pushPrefEnv(
    {
      "set": [
        ["dom.storageManager.enabled", true],
        ["dom.storageManager.prompt.testing", true],
      ]
    },
    nextTestHarnessStep
  );
  yield undefined;

  info("Clearing old databases");

  clearAllDatabases(nextTestHarnessStep);
  yield undefined;

  info("Running" +
       (testScriptFilename ? " '" + testScriptFilename + "'" : ""));

  if (testScriptFilename && !window.disableWorkerTest) {
    info("Running test in a worker");

    let workerScriptBlob =
      new Blob([ "(" + workerScript.toString() + ")();" ],
               { type: "text/javascript" });
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
          ok(message.condition, message.name, message.diag);
          break;

        case "todo":
          todo(message.condition, message.name, message.diag);
          break;

        case "info":
          info(message.msg);
          break;

        case "ready":
          worker.postMessage({ op: "load", files: [ testScriptPath ] });
          break;

        case "loaded":
          worker.postMessage({ op: "start" });
          break;

        case "done":
          ok(true, "Worker finished");
          nextTestHarnessStep();
          break;

        case "clearAllDatabases":
          clearAllDatabases(function(){
            worker.postMessage({ op: "clearAllDatabasesDone" });
          });
          break;

        default:
          ok(false,
             "Received a bad message from worker: " + JSON.stringify(message));
          nextTestHarnessStep();
      }
    };

    URL.revokeObjectURL(workerScriptURL);

    yield undefined;

    worker.terminate();
    worker = null;

    clearAllDatabases(nextTestHarnessStep);
    yield undefined;
  } else if (testScriptFilename) {
    todo(false,
         "Skipping test in a worker because it is explicitly disabled: " +
         disableWorkerTest);
  } else {
    todo(false,
         "Skipping test in a worker because it's not structured properly");
  }

  info("Running test in main thread");

  // Now run the test script in the main thread.
  testGenerator.next();

  yield undefined;
}

if (!window.runTest) {
  window.runTest = function()
  {
    SimpleTest.waitForExplicitFinish();
    testHarnessGenerator.next();
  }
}

function finishTest()
{
  SimpleTest.executeSoon(function() {
    clearAllDatabases(function() { SimpleTest.finish(); });
  });
}

function grabArgAndContinueHandler(arg)
{
  testGenerator.next(arg);
}

function continueToNextStep()
{
  SimpleTest.executeSoon(function() {
    testGenerator.next();
  });
}

function continueToNextStepSync()
{
  testGenerator.next();
}

function workerScript()
{
  "use strict";

  self.repr = function(_thing_) {
    if (typeof(_thing_) == "undefined") {
      return "undefined";
    }

    let str;

    try {
      str = _thing_ + "";
    } catch (e) {
      return "[" + typeof(_thing_) + "]";
    }

    if (typeof(_thing_) == "function") {
      str = str.replace(/^\s+/, "");
      let idx = str.indexOf("{");
      if (idx != -1) {
        str = str.substr(0, idx) + "{...}";
      }
    }

    return str;
  };

  self.ok = function(_condition_, _name_, _diag_) {
    self.postMessage({ op: "ok",
                       condition: !!_condition_,
                       name: _name_,
                       diag: _diag_ });
  };

  self.is = function(_a_, _b_, _name_) {
    let pass = (_a_ == _b_);
    let diag = pass ? "" : "got " + repr(_a_) + ", expected " + repr(_b_);
    ok(pass, _name_, diag);
  };

  self.isnot = function(_a_, _b_, _name_) {
    let pass = (_a_ != _b_);
    let diag = pass ? "" : "didn't expect " + repr(_a_) + ", but got it";
    ok(pass, _name_, diag);
  };

  self.todo = function(_condition_, _name_, _diag_) {
    self.postMessage({ op: "todo",
                       condition: !!_condition_,
                       name: _name_,
                       diag: _diag_ });
  };

  self.info = function(_msg_) {
    self.postMessage({ op: "info", msg: _msg_ });
  };

  self.executeSoon = function(_fun_) {
    var channel = new MessageChannel();
    channel.port1.postMessage("");
    channel.port2.onmessage = function(event) { _fun_(); };
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
  }

  self.onerror = function(_message_, _file_, _line_) {
    ok(false,
       "Worker: uncaught exception [" + _file_ + ":" + _line_ + "]: '" +
         _message_ + "'");
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
        throw new Error("Received a bad message from parent: " +
                        JSON.stringify(message));
    }
  };

  self.postMessage({ op: "ready" });
}
