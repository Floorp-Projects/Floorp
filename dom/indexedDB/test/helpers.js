/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// testSteps is expected to be defined by the test using this file.
/* global testSteps:false */

var testGenerator;
if (testSteps.constructor.name === "GeneratorFunction") {
  testGenerator = testSteps();
}
// The test js is shared between xpcshell (which has no SpecialPowers object)
// and content mochitests (where the |Components| object is accessible only as
// SpecialPowers.Components). Expose Components if necessary here to make things
// work everywhere.
//
// Even if the real |Components| doesn't exist, we might shim in a simple JS
// placebo for compat. An easy way to differentiate this from the real thing
// is whether the property is read-only or not.
var c = Object.getOwnPropertyDescriptor(this, "Components");
if ((!c || !c.value || c.writable) && typeof SpecialPowers === "object") {
  // eslint-disable-next-line no-global-assign
  Components = SpecialPowers.wrap(SpecialPowers.Components);
}

function executeSoon(aFun) {
  SpecialPowers.Services.tm.dispatchToMainThread({
    run() {
      aFun();
    },
  });
}

function clearAllDatabases(callback) {
  let qms = SpecialPowers.Services.qms;
  let principal = SpecialPowers.wrap(document).effectiveStoragePrincipal;
  let request = qms.clearStoragesForPrincipal(principal);
  let cb = SpecialPowers.wrapCallback(callback);
  request.callback = cb;
}

var testHarnessGenerator = testHarnessSteps();
testHarnessGenerator.next();

function* testHarnessSteps() {
  function nextTestHarnessStep(val) {
    testHarnessGenerator.next(val);
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

  info("Running" + (testScriptFilename ? " '" + testScriptFilename + "'" : ""));

  info("Pushing preferences");

  SpecialPowers.pushPrefEnv(
    {
      set: [
        ["dom.indexedDB.testing", true],
        ["dom.indexedDB.experimental", true],
        ["javascript.options.wasm_baselinejit", true], // This can be removed when on by default
      ],
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
        context: document,
      },
    ],
    nextTestHarnessStep
  );
  yield undefined;

  info("Clearing old databases");

  clearAllDatabases(nextTestHarnessStep);
  yield undefined;

  if (testScriptFilename && !window.disableWorkerTest) {
    // For the AsyncFunction, handle the executing sequece using
    // add_task(). For the GeneratorFunction, we just handle the sequence
    // manually.
    if (testSteps.constructor.name === "AsyncFunction") {
      add_task(function workerTestSteps() {
        return executeWorkerTestAndCleanUp(testScriptPath);
      });
    } else {
      ok(
        testSteps.constructor.name === "GeneratorFunction",
        "Unsupported function type"
      );
      executeWorkerTestAndCleanUp(testScriptPath).then(nextTestHarnessStep);

      yield undefined;
    }
  } else if (testScriptFilename) {
    todo(
      false,
      "Skipping test in a worker because it is explicitly disabled: " +
        window.disableWorkerTest
    );
  } else {
    todo(
      false,
      "Skipping test in a worker because it's not structured properly"
    );
  }

  info("Running test in main thread");

  // Now run the test script in the main thread.
  if (testSteps.constructor.name === "AsyncFunction") {
    // Register a callback to clean up databases because it's the only way for
    // add_task() to clean them right before the SimpleTest.FinishTest
    SimpleTest.registerCleanupFunction(async function() {
      await new Promise(function(resolve, reject) {
        clearAllDatabases(function(result) {
          if (result.resultCode == SpecialPowers.Cr.NS_OK) {
            resolve(result);
          } else {
            reject(result.resultCode);
          }
        });
      });
    });

    add_task(testSteps);
  } else {
    testGenerator.next();

    yield undefined;
  }
}

if (!window.runTest) {
  window.runTest = function() {
    SimpleTest.waitForExplicitFinish();
    testHarnessGenerator.next();
  };
}

function finishTest() {
  ok(
    testSteps.constructor.name === "GeneratorFunction",
    "Async/await tests shouldn't call finishTest()"
  );
  SimpleTest.executeSoon(function() {
    clearAllDatabases(function() {
      SimpleTest.finish();
    });
  });
}

function browserRunTest() {
  testGenerator.next();
}

function browserFinishTest() {}

function grabEventAndContinueHandler(event) {
  testGenerator.next(event);
}

function continueToNextStep() {
  SimpleTest.executeSoon(function() {
    testGenerator.next();
  });
}

function continueToNextStepSync() {
  testGenerator.next();
}

function errorHandler(event) {
  ok(false, "indexedDB error, '" + event.target.error.name + "'");
  finishTest();
}

// For error callbacks where the argument is not an event object.
function errorCallbackHandler(err) {
  ok(false, "got unexpected error callback: " + err);
  finishTest();
}

function expectUncaughtException(expecting) {
  SimpleTest.expectUncaughtException(expecting);
}

function browserErrorHandler(event) {
  browserFinishTest();
  throw new Error("indexedDB error (" + event.code + "): " + event.message);
}

function unexpectedSuccessHandler() {
  ok(false, "Got success, but did not expect it!");
  finishTest();
}

function expectedErrorHandler(name) {
  return function(event) {
    is(event.type, "error", "Got an error event");
    is(event.target.error.name, name, "Expected error was thrown.");
    event.preventDefault();
    grabEventAndContinueHandler(event);
  };
}

function ExpectError(name, preventDefault) {
  this._name = name;
  this._preventDefault = preventDefault;
}
ExpectError.prototype = {
  handleEvent(event) {
    is(event.type, "error", "Got an error event");
    is(event.target.error.name, this._name, "Expected error was thrown.");
    if (this._preventDefault) {
      event.preventDefault();
      event.stopPropagation();
    }
    grabEventAndContinueHandler(event);
  },
};

function compareKeys(_k1_, _k2_) {
  let t = typeof _k1_;
  if (t != typeof _k2_) {
    return false;
  }

  if (t !== "object") {
    return _k1_ === _k2_;
  }

  if (_k1_ instanceof Date) {
    return _k2_ instanceof Date && _k1_.getTime() === _k2_.getTime();
  }

  if (_k1_ instanceof Array) {
    if (!(_k2_ instanceof Array) || _k1_.length != _k2_.length) {
      return false;
    }

    for (let i = 0; i < _k1_.length; ++i) {
      if (!compareKeys(_k1_[i], _k2_[i])) {
        return false;
      }
    }

    return true;
  }

  if (_k1_ instanceof ArrayBuffer) {
    if (!(_k2_ instanceof ArrayBuffer)) {
      return false;
    }

    function arrayBuffersAreEqual(a, b) {
      if (a.byteLength != b.byteLength) {
        return false;
      }
      let ui8b = new Uint8Array(b);
      return new Uint8Array(a).every((val, i) => val === ui8b[i]);
    }

    return arrayBuffersAreEqual(_k1_, _k2_);
  }

  return false;
}

function removePermission(type, url) {
  if (!url) {
    url = window.document;
  }
  SpecialPowers.removePermission(type, url);
}

function gc() {
  SpecialPowers.forceGC();
  SpecialPowers.forceCC();
}

function scheduleGC() {
  SpecialPowers.exactGC(continueToNextStep);
}

// Assert that eventually a condition becomes true, running a garbage
// collection between evaluations. Fails, after a high number of iterations
// without a successful evaluation of the condition.
function* assertEventuallyWithGC(conditionFunctor, message) {
  const maxGC = 100;
  for (let i = 0; i < maxGC; ++i) {
    if (conditionFunctor()) {
      ok(true, message + " (after " + i + " garbage collections)");
      return;
    }
    SpecialPowers.exactGC(continueToNextStep);
    yield undefined;
  }
  ok(false, message + " (even after " + maxGC + " garbage collections)");
}

// Asserts that a functor `f` throws an exception that is an instance of
// `ctor`. If it doesn't throw, or throws a different type of exception, this
// throws an Error, including the optional `msg` given.
// Otherwise, it returns the message of the exception.
//
// TODO This is DUPLICATED from https://searchfox.org/mozilla-central/rev/cfd1cc461f1efe0d66c2fdc17c024a203d5a2fd8/js/src/tests/shell.js#163
// This should be moved to a more generic place, as it is in no way specific
// to IndexedDB.
function assertThrowsInstanceOf(f, ctor, msg) {
  var fullmsg;
  try {
    f();
  } catch (exc) {
    if (exc instanceof ctor) {
      return exc.message;
    }
    fullmsg = `Assertion failed: expected exception ${ctor.name}, got ${exc}`;
  }

  if (fullmsg === undefined) {
    fullmsg = `Assertion failed: expected exception ${ctor.name}, no exception thrown`;
  }
  if (msg !== undefined) {
    fullmsg += " - " + msg;
  }

  throw new Error(fullmsg);
}

function isWasmSupported() {
  let testingFunctions = SpecialPowers.Cu.getJSTestingFunctions();
  return testingFunctions.wasmIsSupported();
}

function getWasmModule(_binary_) {
  let module = new WebAssembly.Module(_binary_);
  return module;
}

function expectingSuccess(request) {
  return new Promise(function(resolve, reject) {
    request.onerror = function(event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      reject(event);
    };
    request.onsuccess = function(event) {
      resolve(event);
    };
    request.onupgradeneeded = function(event) {
      ok(false, "Got upgrade, but did not expect it!");
      reject(event);
    };
  });
}

function expectingUpgrade(request) {
  return new Promise(function(resolve, reject) {
    request.onerror = function(event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      reject(event);
    };
    request.onupgradeneeded = function(event) {
      resolve(event);
    };
    request.onsuccess = function(event) {
      ok(false, "Got success, but did not expect it!");
      reject(event);
    };
  });
}

function expectingError(request, errorName) {
  return new Promise(function(resolve, reject) {
    request.onerror = function(event) {
      is(errorName, event.target.error.name, "Correct exception type");
      event.stopPropagation();
      resolve(event);
    };
    request.onsuccess = function(event) {
      ok(false, "Got success, but did not expect it!");
      reject(event);
    };
    request.onupgradeneeded = function(event) {
      ok(false, "Got upgrade, but did not expect it!");
      reject(event);
    };
  });
}

function workerScript() {
  "use strict";

  self.wasmSupported = false;

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
    self.ok(
      testSteps.constructor.name === "GeneratorFunction",
      "Async/await tests shouldn't call finishTest()"
    );
    if (self._expectingUncaughtException) {
      self.ok(
        false,
        "expectUncaughtException was called but no uncaught " +
          "exception was detected!"
      );
    }
    self.postMessage({ op: "done" });
  };

  self.grabEventAndContinueHandler = function(_event_) {
    testGenerator.next(_event_);
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

  self.unexpectedSuccessHandler = function() {
    ok(false, "Got success, but did not expect it!");
    finishTest();
  };

  self.expectedErrorHandler = function(_name_) {
    return function(_event_) {
      is(_event_.type, "error", "Got an error event");
      is(_event_.target.error.name, _name_, "Expected error was thrown.");
      _event_.preventDefault();
      grabEventAndContinueHandler(_event_);
    };
  };

  self.ExpectError = function(_name_, _preventDefault_) {
    this._name = _name_;
    this._preventDefault = _preventDefault_;
  };
  self.ExpectError.prototype = {
    handleEvent(_event_) {
      is(_event_.type, "error", "Got an error event");
      is(_event_.target.error.name, this._name, "Expected error was thrown.");
      if (this._preventDefault) {
        _event_.preventDefault();
        _event_.stopPropagation();
      }
      grabEventAndContinueHandler(_event_);
    },
  };

  // TODO this is duplicate from the global compareKeys function defined above,
  // this duplication should be avoided (bug 1565986)
  self.compareKeys = function(_k1_, _k2_) {
    let t = typeof _k1_;
    if (t != typeof _k2_) {
      return false;
    }

    if (t !== "object") {
      return _k1_ === _k2_;
    }

    if (_k1_ instanceof Date) {
      return _k2_ instanceof Date && _k1_.getTime() === _k2_.getTime();
    }

    if (_k1_ instanceof Array) {
      if (!(_k2_ instanceof Array) || _k1_.length != _k2_.length) {
        return false;
      }

      for (let i = 0; i < _k1_.length; ++i) {
        if (!compareKeys(_k1_[i], _k2_[i])) {
          return false;
        }
      }

      return true;
    }

    if (_k1_ instanceof ArrayBuffer) {
      if (!(_k2_ instanceof ArrayBuffer)) {
        return false;
      }

      function arrayBuffersAreEqual(a, b) {
        if (a.byteLength != b.byteLength) {
          return false;
        }
        let ui8b = new Uint8Array(b);
        return new Uint8Array(a).every((val, i) => val === ui8b[i]);
      }

      return arrayBuffersAreEqual(_k1_, _k2_);
    }

    return false;
  };

  self.getRandomBuffer = function(_size_) {
    let buffer = new ArrayBuffer(_size_);
    is(buffer.byteLength, _size_, "Correct byte length");
    let view = new Uint8Array(buffer);
    for (let i = 0; i < _size_; i++) {
      view[i] = parseInt(Math.random() * 255);
    }
    return buffer;
  };

  self._expectingUncaughtException = false;
  self.expectUncaughtException = function(_expecting_) {
    self._expectingUncaughtException = !!_expecting_;
    self.postMessage({
      op: "expectUncaughtException",
      expecting: !!_expecting_,
    });
  };

  self._clearAllDatabasesCallback = undefined;
  self.clearAllDatabases = function(_callback_) {
    self._clearAllDatabasesCallback = _callback_;
    self.postMessage({ op: "clearAllDatabases" });
  };

  self.onerror = function(_message_, _file_, _line_) {
    if (self._expectingUncaughtException) {
      self._expectingUncaughtException = false;
      ok(
        true,
        "Worker: expected exception [" +
          _file_ +
          ":" +
          _line_ +
          "]: '" +
          _message_ +
          "'"
      );
      return false;
    }
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

  self.isWasmSupported = function() {
    return self.wasmSupported;
  };

  self.getWasmModule = function(_binary_) {
    let module = new WebAssembly.Module(_binary_);
    return module;
  };

  self.verifyWasmModule = function(_module) {
    self.todo(false, "Need a verifyWasmModule implementation on workers");
    self.continueToNextStep();
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
        self.wasmSupported = message.wasmSupported;
        executeSoon(async function() {
          info("Worker: starting tests");
          if (testSteps.constructor.name === "AsyncFunction") {
            await testSteps();
            if (self._expectingUncaughtException) {
              self.ok(
                false,
                "expectUncaughtException was called but no " +
                  "uncaught exception was detected!"
              );
            }
            self.postMessage({ op: "done" });
          } else {
            ok(
              testSteps.constructor.name === "GeneratorFunction",
              "Unsupported function type"
            );
            testGenerator.next();
          }
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

  self.expectingSuccess = function(_request_) {
    return new Promise(function(_resolve_, _reject_) {
      _request_.onerror = function(_event_) {
        ok(false, "indexedDB error, '" + _event_.target.error.name + "'");
        _reject_(_event_);
      };
      _request_.onsuccess = function(_event_) {
        _resolve_(_event_);
      };
      _request_.onupgradeneeded = function(_event_) {
        ok(false, "Got upgrade, but did not expect it!");
        _reject_(_event_);
      };
    });
  };

  self.expectingUpgrade = function(_request_) {
    return new Promise(function(_resolve_, _reject_) {
      _request_.onerror = function(_event_) {
        ok(false, "indexedDB error, '" + _event_.target.error.name + "'");
        _reject_(_event_);
      };
      _request_.onupgradeneeded = function(_event_) {
        _resolve_(_event_);
      };
      _request_.onsuccess = function(_event_) {
        ok(false, "Got success, but did not expect it!");
        _reject_(_event_);
      };
    });
  };

  self.postMessage({ op: "ready" });
}

async function executeWorkerTestAndCleanUp(testScriptPath) {
  info("Running test in a worker");

  let workerScriptBlob = new Blob(["(" + workerScript.toString() + ")();"], {
    type: "text/javascript",
  });
  let workerScriptURL = URL.createObjectURL(workerScriptBlob);

  let worker;
  try {
    await new Promise(function(resolve, reject) {
      worker = new Worker(workerScriptURL);

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
        reject();
      };

      worker.onmessage = function(event) {
        let message = event.data;
        switch (message.op) {
          case "ok":
            SimpleTest.ok(
              message.condition,
              `${message.name}: ${message.diag}`
            );
            break;

          case "todo":
            todo(message.condition, message.name, message.diag);
            break;

          case "info":
            info(message.msg);
            break;

          case "ready":
            worker.postMessage({ op: "load", files: [testScriptPath] });
            break;

          case "loaded":
            worker.postMessage({
              op: "start",
              wasmSupported: isWasmSupported(),
            });
            break;

          case "done":
            ok(true, "Worker finished");
            resolve();
            break;

          case "expectUncaughtException":
            worker._expectingUncaughtException = message.expecting;
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
            reject();
        }
      };
    });

    URL.revokeObjectURL(workerScriptURL);
  } catch (e) {
    info("Unexpected thing happened: " + e);
  }

  return new Promise(function(resolve) {
    info("Cleaning up the databases");

    if (worker._expectingUncaughtException) {
      ok(
        false,
        "expectUncaughtException was called but no uncaught " +
          "exception was detected!"
      );
    }

    worker.terminate();
    worker = null;

    clearAllDatabases(resolve);
  });
}
