/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var timer = require("timer");
var file = require("file");

exports.findAndRunTests = function findAndRunTests(options) {
  var finder = new TestFinder(options.dirs);
  var runner = new TestRunner();
  runner.startMany({tests: finder.findTests(),
                    onDone: options.onDone});
};

var TestFinder = exports.TestFinder = function TestFinder(dirs) {
  this.dirs = dirs;
};

TestFinder.prototype = {
  _makeTest: function _makeTest(suite, name, test) {
    function runTest(runner) {
      console.info("executing '" + suite + "." + name + "'");
      test(runner);
    }
    return runTest;
  },

  findTests: function findTests() {
    var self = this;
    var tests = [];

    this.dirs.forEach(
      function(dir) {
        var suites = [name.slice(0, -3)
                      for each (name in file.list(dir))
                      if (/^test-.*\.js$/.test(name))];

        suites.forEach(
          function(suite) {
            var module = require(suite);
            for (name in module)
              if (name.indexOf("test") == 0)
                tests.push(self._makeTest(suite, name, module[name]));
          });
      });
    return tests;
  }
};

var TestRunner = exports.TestRunner = function TestRunner(options) {
  this.passed = 0;
  this.failed = 0;
};

TestRunner.prototype = {
  DEFAULT_PAUSE_TIMEOUT: 10000,

  makeSandboxedLoader: function makeSandboxedLoader(options) {
    if (!options)
      options = {};
    var Cuddlefish = require("cuddlefish");

    options.fs = Cuddlefish.parentLoader.fs;
    return new Cuddlefish.Loader(options);
  },

  pass: function pass(message) {
    console.info("pass:", message);
    this.passed++;
  },

  fail: function fail(message) {
    console.error("fail:", message);
    this.failed++;
  },

  exception: function exception(e) {
    console.exception(e);
    this.failed++;
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
      if (typeof(predicate) == "object")
        this.assertMatches(e.message, predicate, message);
      else
        this.assertEqual(e.message, predicate, message);
    }
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

  done: function done() {
    if (!this.isDone) {
      this.isDone = true;
      if (this.waitTimeout !== null) {
        timer.clearTimeout(this.waitTimeout);
        this.waitTimeout = null;
      }
      if (this.onDone !== null) {
        var onDone = this.onDone;
        this.onDone = null;
        onDone(this);
      }
    }
  },

  waitUntilDone: function waitUntilDone(ms) {
    if (ms === undefined)
      ms = this.DEFAULT_PAUSE_TIMEOUT;

    var self = this;

    function tiredOfWaiting() {
      self.failed++;
      self.done();
    }

    this.waitTimeout = timer.setTimeout(tiredOfWaiting, ms);
  },

  startMany: function startMany(options) {
    function scheduleNextTest(self) {
      function runNextTest() {
        var test = options.tests.pop();
        if (test)
          self.start({test: test, onDone: scheduleNextTest});
        else
          options.onDone(self);
      }
      timer.setTimeout(runNextTest, 0);
    }
    scheduleNextTest(this);
  },

  start: function start(options) {
    this.test = options.test;
    this.isDone = false;
    this.onDone = options.onDone;
    this.waitTimeout = null;

    try {
      this.test(this);
    } catch (e) {
      this.exception(e);
    }
    if (this.waitTimeout === null)
      this.done();
  }
};
