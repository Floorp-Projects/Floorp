/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Jetpack.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Atul Varma <atul@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
