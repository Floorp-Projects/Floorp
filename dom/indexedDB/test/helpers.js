/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();
var archiveReaderEnabled = false;

// The test js is shared between xpcshell (which has no SpecialPowers object)
// and content mochitests (where the |Components| object is accessible only as
// SpecialPowers.Components). Expose Components if necessary here to make things
// work everywhere.
//
// Even if the real |Components| doesn't exist, we might shim in a simple JS
// placebo for compat. An easy way to differentiate this from the real thing
// is whether the property is read-only or not.
var c = Object.getOwnPropertyDescriptor(this, 'Components');
if ((!c.value || c.writable) && typeof SpecialPowers === 'object')
  Components = SpecialPowers.Components;

function executeSoon(aFun)
{
  let comp = SpecialPowers.wrap(Components);

  let thread = comp.classes["@mozilla.org/thread-manager;1"]
                   .getService(comp.interfaces.nsIThreadManager)
                   .mainThread;

  thread.dispatch({
    run: function() {
      aFun();
    }
  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);
}

function clearAllDatabases(callback) {
  let qms = SpecialPowers.Services.qms;
  let principal = SpecialPowers.wrap(document).nodePrincipal;
  let request = qms.clearStoragesForPrincipal(principal);
  let cb = SpecialPowers.wrapCallback(callback);
  request.callback = cb;
}

var testHarnessGenerator = testHarnessSteps();
testHarnessGenerator.next();

function testHarnessSteps() {
  function nextTestHarnessStep(val) {
    testHarnessGenerator.send(val);
  }

  let testScriptPath;
  let testScriptFilename;

  let scripts = document.getElementsByTagName("script");
  for (let i = 0; i < scripts.length; i++) {
    let src = scripts[i].src;
    let match = src.match(/indexedDB\/test\/unit\/(test_[^\/]+\.js)$/);
    if (match && match.length == 2) {
      testScriptPath = src;
      testScriptFilename = match[1];
      break;
    }
  }

  yield undefined;

  info("Running" +
       (testScriptFilename ? " '" + testScriptFilename + "'" : ""));

  info("Pushing preferences");

  SpecialPowers.pushPrefEnv(
    {
      "set": [
        ["dom.indexedDB.testing", true],
        ["dom.indexedDB.experimental", true],
        ["dom.archivereader.enabled", true],
        ["dom.workers.latestJSVersion", true]
      ]
    },
    nextTestHarnessStep
  );
  yield undefined;

  info("Pushing permissions");

  SpecialPowers.pushPermissions(
    [
      {
        type: "indexedDB",
        allow: true,
        context: document
      }
    ],
    nextTestHarnessStep
  );
  yield undefined;

  info("Clearing old databases");

  clearAllDatabases(nextTestHarnessStep);
  yield undefined;

  if (testScriptFilename && !window.disableWorkerTest) {
    info("Running test in a worker");

    let workerScriptBlob =
      new Blob([ "(" + workerScript.toString() + ")();" ],
               { type: "text/javascript;version=1.7" });
    let workerScriptURL = URL.createObjectURL(workerScriptBlob);

    let worker = new Worker(workerScriptURL);

    worker._expectingUncaughtException = false;
    worker.onerror = function(event) {
      if (worker._expectingUncaughtException) {
        ok(true, "Worker had an expected error: " + event.message);
        worker._expectingUncaughtException = false;
        event.preventDefault();
        return;
      }
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

        case "expectUncaughtException":
          worker._expectingUncaughtException = message.expecting;
          break;

        default:
          ok(false,
             "Received a bad message from worker: " + JSON.stringify(message));
          nextTestHarnessStep();
      }
    };

    URL.revokeObjectURL(workerScriptURL);

    yield undefined;

    if (worker._expectingUncaughtException) {
      ok(false, "expectUncaughtException was called but no uncaught " +
                "exception was detected!");
    }

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
  SpecialPowers.notifyObserversInParentProcess(null,
                                               "disk-space-watcher",
                                               "free");

  SimpleTest.executeSoon(function() {
    testGenerator.close();
    testHarnessGenerator.close();
    clearAllDatabases(function() { SimpleTest.finish(); });
  });
}

function browserRunTest()
{
  testGenerator.next();
}

function browserFinishTest()
{
  setTimeout(function() { testGenerator.close(); }, 0);
}

function grabEventAndContinueHandler(event)
{
  testGenerator.send(event);
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

function errorHandler(event)
{
  ok(false, "indexedDB error, '" + event.target.error.name + "'");
  finishTest();
}

function expectUncaughtException(expecting)
{
  SimpleTest.expectUncaughtException(expecting);
}

function browserErrorHandler(event)
{
  browserFinishTest();
  throw new Error("indexedDB error (" + event.code + "): " + event.message);
}

function unexpectedSuccessHandler()
{
  ok(false, "Got success, but did not expect it!");
  finishTest();
}

function expectedErrorHandler(name)
{
  return function(event) {
    is(event.type, "error", "Got an error event");
    is(event.target.error.name, name, "Expected error was thrown.");
    event.preventDefault();
    grabEventAndContinueHandler(event);
  };
}

function ExpectError(name, preventDefault)
{
  this._name = name;
  this._preventDefault = preventDefault;
}
ExpectError.prototype = {
  handleEvent: function(event)
  {
    is(event.type, "error", "Got an error event");
    is(event.target.error.name, this._name, "Expected error was thrown.");
    if (this._preventDefault) {
      event.preventDefault();
      event.stopPropagation();
    }
    grabEventAndContinueHandler(event);
  }
};

function compareKeys(_k1_, _k2_) {
  let t = typeof _k1_;
  if (t != typeof _k2_)
    return false;

  if (t !== "object")
    return _k1_ === _k2_;

  if (_k1_ instanceof Date) {
    return (_k2_ instanceof Date) &&
      _k1_.getTime() === _k2_.getTime();
  }

  if (_k1_ instanceof Array) {
    if (!(_k2_ instanceof Array) ||
        _k1_.length != _k2_.length)
      return false;

    for (let i = 0; i < _k1_.length; ++i) {
      if (!compareKeys(_k1_[i], _k2_[i]))
        return false;
    }

    return true;
  }

  return false;
}

function removePermission(type, url)
{
  if (!url) {
    url = window.document;
  }
  SpecialPowers.removePermission(type, url);
}

function gc()
{
  SpecialPowers.forceGC();
  SpecialPowers.forceCC();
}

function scheduleGC()
{
  SpecialPowers.exactGC(window, continueToNextStep);
}

function workerScript() {
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
    if (self._expectingUncaughtException) {
      self.ok(false, "expectUncaughtException was called but no uncaught "
                     + "exception was detected!");
    }
    self.postMessage({ op: "done" });
  };

  self.grabEventAndContinueHandler = function(_event_) {
    testGenerator.send(_event_);
  };

  self.continueToNextStep = function() {
    executeSoon(function() {
      testGenerator.next();
    });
  };

  self.continueToNextStepSync = function() {
    testGenerator.next();
  };

  self.errorHandler = function(_event_) {
    ok(false, "indexedDB error, '" + _event_.target.error.name + "'");
    finishTest();
  };

  self.unexpectedSuccessHandler = function()
  {
    ok(false, "Got success, but did not expect it!");
    finishTest();
  };

  self.expectedErrorHandler = function(_name_)
  {
    return function(_event_) {
      is(_event_.type, "error", "Got an error event");
      is(_event_.target.error.name, _name_, "Expected error was thrown.");
      _event_.preventDefault();
      grabEventAndContinueHandler(_event_);
    };
  };

  self.ExpectError = function(_name_, _preventDefault_)
  {
    this._name = _name_;
    this._preventDefault = _preventDefault_;
  }
  self.ExpectError.prototype = {
    handleEvent: function(_event_)
    {
      is(_event_.type, "error", "Got an error event");
      is(_event_.target.error.name, this._name, "Expected error was thrown.");
      if (this._preventDefault) {
        _event_.preventDefault();
        _event_.stopPropagation();
      }
      grabEventAndContinueHandler(_event_);
    }
  };

  self.compareKeys = function(_k1_, _k2_) {
    let t = typeof _k1_;
    if (t != typeof _k2_)
      return false;

    if (t !== "object")
      return _k1_ === _k2_;

    if (_k1_ instanceof Date) {
      return (_k2_ instanceof Date) &&
        _k1_.getTime() === _k2_.getTime();
    }

    if (_k1_ instanceof Array) {
      if (!(_k2_ instanceof Array) ||
          _k1_.length != _k2_.length)
        return false;

      for (let i = 0; i < _k1_.length; ++i) {
        if (!compareKeys(_k1_[i], _k2_[i]))
          return false;
      }

      return true;
    }

    return false;
  }

  self.getRandomBuffer = function(_size_) {
    let buffer = new ArrayBuffer(_size_);
    is(buffer.byteLength, _size_, "Correct byte length");
    let view = new Uint8Array(buffer);
    for (let i = 0; i < _size_; i++) {
      view[i] = parseInt(Math.random() * 255)
    }
    return buffer;
  };

  self._expectingUncaughtException = false;
  self.expectUncaughtException = function(_expecting_) {
    self._expectingUncaughtException = !!_expecting_;
    self.postMessage({ op: "expectUncaughtException", expecting: !!_expecting_ });
  };

  self.onerror = function(_message_, _file_, _line_) {
    if (self._expectingUncaughtException) {
      self._expectingUncaughtException = false;
      ok(true, "Worker: expected exception [" + _file_ + ":" + _line_ + "]: '" +
         _message_ + "'");
      return;
    }
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

      default:
        throw new Error("Received a bad message from parent: " +
                        JSON.stringify(message));
    }
  };

  self.postMessage({ op: "ready" });
}
