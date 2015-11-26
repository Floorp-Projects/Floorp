/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "deprecated"
};

const timer = require("../timers");
const cfxArgs = require("../test/options");
const { getTabs, closeTab, getURI, getTabId, getSelectedTab } = require("../tabs/utils");
const { windows, isBrowser, getMostRecentBrowserWindow } = require("../window/utils");
const { defer, all, Debugging: PromiseDebugging, resolve } = require("../core/promise");
const { getInnerId } = require("../window/utils");
const { cleanUI } = require("../test/utils");

const findAndRunTests = function findAndRunTests(options) {
  var TestFinder = require("./unit-test-finder").TestFinder;
  var finder = new TestFinder({
    filter: options.filter,
    testInProcess: options.testInProcess,
    testOutOfProcess: options.testOutOfProcess
  });
  var runner = new TestRunner({fs: options.fs});
  finder.findTests().then(tests => {
    runner.startMany({
      tests: tests,
      stopOnError: options.stopOnError,
      onDone: options.onDone
    });
  });
};
exports.findAndRunTests = findAndRunTests;

var runnerWindows = new WeakMap();
var runnerTabs = new WeakMap();

const TestRunner = function TestRunner(options) {
  options = options || {};

  // remember the id's for the open window and tab
  let window = getMostRecentBrowserWindow();
  runnerWindows.set(this, getInnerId(window));
  runnerTabs.set(this, getTabId(getSelectedTab(window)));

  this.fs = options.fs;
  this.console = options.console || console;
  this.passed = 0;
  this.failed = 0;
  this.testRunSummary = [];
  this.expectFailNesting = 0;
  this.done = TestRunner.prototype.done.bind(this);
};

TestRunner.prototype = {
  toString: function toString() {
    return "[object TestRunner]";
  },

  DEFAULT_PAUSE_TIMEOUT: (cfxArgs.parseable ? 300000 : 15000), //Five minutes (5*60*1000ms)
  PAUSE_DELAY: 500,

  _logTestFailed: function _logTestFailed(why) {
    if (!(why in this.test.errors))
      this.test.errors[why] = 0;
    this.test.errors[why]++;
  },

  _uncaughtErrorObserver: function({message, date, fileName, stack, lineNumber}) {
    this.fail("There was an uncaught Promise rejection: " + message + " @ " +
              fileName + ":" + lineNumber + "\n" + stack);
  },

  pass: function pass(message) {
    if(!this.expectFailure) {
      if ("testMessage" in this.console)
        this.console.testMessage(true, true, this.test.name, message);
      else
        this.console.info("pass:", message);
      this.passed++;
      this.test.passed++;
      this.test.last = message;
    }
    else {
      this.expectFailure = false;
      this._logTestFailed("failure");
      if ("testMessage" in this.console) {
        this.console.testMessage(true, false, this.test.name, message);
      }
      else {
        this.console.error("fail:", 'Failure Expected: ' + message)
        this.console.trace();
      }
      this.failed++;
      this.test.failed++;
    }
  },

  fail: function fail(message) {
    if(!this.expectFailure) {
      this._logTestFailed("failure");
      if ("testMessage" in this.console) {
        this.console.testMessage(false, false, this.test.name, message);
      }
      else {
        this.console.error("fail:", message)
        this.console.trace();
      }
      this.failed++;
      this.test.failed++;
    }
    else {
      this.expectFailure = false;
      if ("testMessage" in this.console)
        this.console.testMessage(false, true, this.test.name, message);
      else
        this.console.info("pass:", message);
      this.passed++;
      this.test.passed++;
      this.test.last = message;
    }
  },

  expectFail: function(callback) {
    this.expectFailure = true;
    callback();
    this.expectFailure = false;
  },

  exception: function exception(e) {
    this._logTestFailed("exception");
    if (cfxArgs.parseable)
      this.console.print("TEST-UNEXPECTED-FAIL | " + this.test.name + " | " + e + "\n");
    this.console.exception(e);
    this.failed++;
    this.test.failed++;
  },

  assertMatches: function assertMatches(string, regexp, message) {
    if (regexp.test(string)) {
      if (!message)
        message = uneval(string) + " matches " + uneval(regexp);
      this.pass(message);
    } else {
      var no = uneval(string) + " doesn't match " + uneval(regexp);
      if (!message)
        message = no;
      else
        message = message + " (" + no + ")";
      this.fail(message);
    }
  },

  assertRaises: function assertRaises(func, predicate, message) {
    try {
      func();
      if (message)
        this.fail(message + " (no exception thrown)");
      else
        this.fail("function failed to throw exception");
    } catch (e) {
      var errorMessage;
      if (typeof(e) == "string")
        errorMessage = e;
      else
        errorMessage = e.message;
      if (typeof(predicate) == "string")
        this.assertEqual(errorMessage, predicate, message);
      else
        this.assertMatches(errorMessage, predicate, message);
    }
  },

  assert: function assert(a, message) {
    if (!a) {
      if (!message)
        message = "assertion failed, value is " + a;
      this.fail(message);
    } else
      this.pass(message || "assertion successful");
  },

  assertNotEqual: function assertNotEqual(a, b, message) {
    if (a != b) {
      if (!message)
        message = "a != b != " + uneval(a);
      this.pass(message);
    } else {
      var equality = uneval(a) + " == " + uneval(b);
      if (!message)
        message = equality;
      else
        message += " (" + equality + ")";
      this.fail(message);
    }
  },

  assertEqual: function assertEqual(a, b, message) {
    if (a == b) {
      if (!message)
        message = "a == b == " + uneval(a);
      this.pass(message);
    } else {
      var inequality = uneval(a) + " != " + uneval(b);
      if (!message)
        message = inequality;
      else
        message += " (" + inequality + ")";
      this.fail(message);
    }
  },

  assertNotStrictEqual: function assertNotStrictEqual(a, b, message) {
    if (a !== b) {
      if (!message)
        message = "a !== b !== " + uneval(a);
      this.pass(message);
    } else {
      var equality = uneval(a) + " === " + uneval(b);
      if (!message)
        message = equality;
      else
        message += " (" + equality + ")";
      this.fail(message);
    }
  },

  assertStrictEqual: function assertStrictEqual(a, b, message) {
    if (a === b) {
      if (!message)
        message = "a === b === " + uneval(a);
      this.pass(message);
    } else {
      var inequality = uneval(a) + " !== " + uneval(b);
      if (!message)
        message = inequality;
      else
        message += " (" + inequality + ")";
      this.fail(message);
    }
  },

  assertFunction: function assertFunction(a, message) {
    this.assertStrictEqual('function', typeof a, message);
  },

  assertUndefined: function(a, message) {
    this.assertStrictEqual('undefined', typeof a, message);
  },

  assertNotUndefined: function(a, message) {
    this.assertNotStrictEqual('undefined', typeof a, message);
  },

  assertNull: function(a, message) {
    this.assertStrictEqual(null, a, message);
  },

  assertNotNull: function(a, message) {
    this.assertNotStrictEqual(null, a, message);
  },

  assertObject: function(a, message) {
    this.assertStrictEqual('[object Object]', Object.prototype.toString.apply(a), message);
  },

  assertString: function(a, message) {
    this.assertStrictEqual('[object String]', Object.prototype.toString.apply(a), message);
  },

  assertArray: function(a, message) {
    this.assertStrictEqual('[object Array]', Object.prototype.toString.apply(a), message);
  },

  assertNumber: function(a, message) {
    this.assertStrictEqual('[object Number]', Object.prototype.toString.apply(a), message);
  },

  done: function done() {
    if (this.isDone) {
      return resolve();
    }

    this.isDone = true;
    this.pass("This test is done.");

    if (this.test.teardown) {
      this.test.teardown(this);
    }

    if (this.waitTimeout !== null) {
      timer.clearTimeout(this.waitTimeout);
      this.waitTimeout = null;
    }

    // Do not leave any callback set when calling to `waitUntil`
    this.waitUntilCallback = null;
    if (this.test.passed == 0 && this.test.failed == 0) {
      this._logTestFailed("empty test");

      if ("testMessage" in this.console) {
        this.console.testMessage(false, false, this.test.name, "Empty test");
      }
      else {
        this.console.error("fail:", "Empty test")
      }

      this.failed++;
      this.test.failed++;
    }

    let wins = windows(null, { includePrivate: true });
    let winPromises = wins.map(win => {
      return new Promise(resolve => {
        if (["interactive", "complete"].indexOf(win.document.readyState) >= 0) {
          resolve()
        }
        else {
          win.addEventListener("DOMContentLoaded", function onLoad() {
            win.removeEventListener("DOMContentLoaded", onLoad, false);
            resolve();
          }, false);
        }
      });
    });

    PromiseDebugging.flushUncaughtErrors();
    PromiseDebugging.removeUncaughtErrorObserver(this._uncaughtErrorObserver);


    return all(winPromises).then(() => {
      let browserWins = wins.filter(isBrowser);
      let tabs = browserWins.reduce((tabs, window) => tabs.concat(getTabs(window)), []);
      let newTabID = getTabId(getSelectedTab(wins[0]));
      let oldTabID = runnerTabs.get(this);
      let hasMoreTabsOpen = browserWins.length && tabs.length != 1;
      let failure = false;

      if (wins.length != 1 || getInnerId(wins[0]) !== runnerWindows.get(this)) {
        failure = true;
        this.fail("Should not be any unexpected windows open");
      }
      else if (hasMoreTabsOpen) {
        failure = true;
        this.fail("Should not be any unexpected tabs open");
      }
      else if (oldTabID != newTabID) {
        failure = true;
        runnerTabs.set(this, newTabID);
        this.fail("Should not be any new tabs left open, old id: " + oldTabID + " new id: " + newTabID);
      }

      if (failure) {
        console.log("Windows open:");
        for (let win of wins) {
          if (isBrowser(win)) {
            tabs = getTabs(win);
            console.log(win.location + " - " + tabs.map(getURI).join(", "));
          }
          else {
            console.log(win.location);
          }
        }
      }

      return failure;
    }).
    then(failure => {
      if (!failure) {
        this.pass("There was a clean UI.");
        return null;
      }
      return cleanUI().then(() => {
        this.pass("There is a clean UI.");
      });
    }).
    then(() => {
      this.testRunSummary.push({
        name: this.test.name,
        passed: this.test.passed,
        failed: this.test.failed,
        errors: Object.keys(this.test.errors).join(", ")
      });

      if (this.onDone !== null) {
        let onDone = this.onDone;
        this.onDone = null;
        timer.setTimeout(_ => onDone(this));
      }
    }).
    catch(console.exception);
  },

  // Set of assertion functions to wait for an assertion to become true
  // These functions take the same arguments as the TestRunner.assert* methods.
  waitUntil: function waitUntil() {
    return this._waitUntil(this.assert, arguments);
  },

  waitUntilNotEqual: function waitUntilNotEqual() {
    return this._waitUntil(this.assertNotEqual, arguments);
  },

  waitUntilEqual: function waitUntilEqual() {
    return this._waitUntil(this.assertEqual, arguments);
  },

  waitUntilMatches: function waitUntilMatches() {
    return this._waitUntil(this.assertMatches, arguments);
  },

  /**
   * Internal function that waits for an assertion to become true.
   * @param {Function} assertionMethod
   *    Reference to a TestRunner assertion method like test.assert,
   *    test.assertEqual, ...
   * @param {Array} args
   *    List of arguments to give to the previous assertion method.
   *    All functions in this list are going to be called to retrieve current
   *    assertion values.
   */
  _waitUntil: function waitUntil(assertionMethod, args) {
    let { promise, resolve } = defer();
    let count = 0;
    let maxCount = this.DEFAULT_PAUSE_TIMEOUT / this.PAUSE_DELAY;

    // We need to ensure that test is asynchronous
    if (!this.waitTimeout)
      this.waitUntilDone(this.DEFAULT_PAUSE_TIMEOUT);

    let finished = false;
    let test = this;

    // capture a traceback before we go async.
    let traceback = require("../console/traceback");
    let stack = traceback.get();
    stack.splice(-2, 2);
    let currentWaitStack = traceback.format(stack);
    let timeout = null;

    function loop(stopIt) {
      timeout = null;

      // Build a mockup object to fake TestRunner API and intercept calls to
      // pass and fail methods, in order to retrieve nice error messages
      // and assertion result
      let mock = {
        pass: function (msg) {
          test.pass(msg);
          test.waitUntilCallback = null;
          if (!stopIt)
            resolve();
        },
        fail: function (msg) {
          // If we are called on test timeout, we stop the loop
          // and print which test keeps failing:
          if (stopIt) {
            test.console.error("test assertion never became true:\n",
                               msg + "\n",
                               currentWaitStack);
            if (timeout)
              timer.clearTimeout(timeout);
            return;
          }
          timeout = timer.setTimeout(loop, test.PAUSE_DELAY);
        }
      };

      // Automatically call args closures in order to build arguments for
      // assertion function
      let appliedArgs = [];
      for (let i = 0, l = args.length; i < l; i++) {
        let a = args[i];
        if (typeof a == "function") {
          try {
            a = a();
          }
          catch(e) {
            test.fail("Exception when calling asynchronous assertion: " + e +
                      "\n" + e.stack);
            return resolve();
          }
        }
        appliedArgs.push(a);
      }

      // Finally call assertion function with current assertion values
      assertionMethod.apply(mock, appliedArgs);
    }
    loop();
    this.waitUntilCallback = loop;

    return promise;
  },

  waitUntilDone: function waitUntilDone(ms) {
    if (ms === undefined)
      ms = this.DEFAULT_PAUSE_TIMEOUT;

    var self = this;

    function tiredOfWaiting() {
      self._logTestFailed("timed out");
      if ("testMessage" in self.console) {
        self.console.testMessage(false, false, self.test.name,
          `Test timed out (after: ${self.test.last})`);
      }
      else {
        self.console.error("fail:", `Timed out (after: ${self.test.last})`)
      }
      if (self.waitUntilCallback) {
        self.waitUntilCallback(true);
        self.waitUntilCallback = null;
      }
      self.failed++;
      self.test.failed++;
      self.done();
    }

    // We may already have registered a timeout callback
    if (this.waitTimeout)
      timer.clearTimeout(this.waitTimeout);

    this.waitTimeout = timer.setTimeout(tiredOfWaiting, ms);
  },

  startMany: function startMany(options) {
    function runNextTest(self) {
      let { tests, onDone } = options;

      return tests.getNext().then((test) => {
        if (options.stopOnError && self.test && self.test.failed) {
          self.console.error("aborted: test failed and --stop-on-error was specified");
          onDone(self);
        }
        else if (test) {
          self.start({test: test, onDone: runNextTest});
        }
        else {
          onDone(self);
        }
      });
    }

    return runNextTest(this).catch(console.exception);
  },

  start: function start(options) {
    this.test = options.test;
    this.test.passed = 0;
    this.test.failed = 0;
    this.test.errors = {};
    this.test.last = 'START';
    PromiseDebugging.clearUncaughtErrorObservers();
    this._uncaughtErrorObserver = this._uncaughtErrorObserver.bind(this);
    PromiseDebugging.addUncaughtErrorObserver(this._uncaughtErrorObserver);

    this.isDone = false;
    this.onDone = function(self) {
      if (cfxArgs.parseable)
        self.console.print("TEST-END | " + self.test.name + "\n");
      options.onDone(self);
    }
    this.waitTimeout = null;

    try {
      if (cfxArgs.parseable)
        this.console.print("TEST-START | " + this.test.name + "\n");
      else
        this.console.info("executing '" + this.test.name + "'");

      if(this.test.setup) {
        this.test.setup(this);
      }
      this.test.testFunction(this);
    } catch (e) {
      this.exception(e);
    }
    if (this.waitTimeout === null)
      this.done();
  }
};
exports.TestRunner = TestRunner;
