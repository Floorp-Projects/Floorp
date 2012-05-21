/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var W3CTest = {
  /**
   * Dictionary mapping a test URL to either the string "all", which means that
   * all tests in this file are expected to fail, or a dictionary mapping test
   * names to either the boolean |true|, or the string "debug". The former
   * means that this test is expected to fail in all builds, and the latter
   * that it is only expected to fail in debug builds.
   */
  "expectedFailures": {},

  /**
   * List of test results, needed by TestRunner to update the UI.
   */
  "tests": [],

  /**
   * Reference to the TestRunner object in the parent frame.
   */
  "runner": parent === this ? null : parent.TestRunner || parent.wrappedJSObject.TestRunner,

  /**
   * Prefixes for the error logging. Indexed first by int(todo) and second by
   * int(result).
   */
  "prefixes": [
    ["TEST-UNEXPECTED-FAIL", "TEST-PASS"],
    ["TEST-KNOWN-FAIL", "TEST-UNEXPECTED-PASS"]
  ],

  /**
   * Returns the URL of the current test, relative to the root W3C tests
   * directory. Used as a key into the expectedFailures dictionary.
   */
  "getURL": function() {
    return this.runner.currentTestURL.substring("/tests/dom/imptests/".length);
  },

  /**
   * Lets the test runner know about a test result.
   */
  "_log": function(test) {
    var msg = this.prefixes[+test.todo][+test.result] + " | ";
    if (this.runner.currentTestURL)
      msg += this.runner.currentTestURL;
    msg += " | " + test.message;
    this.runner[(test.result === !test.todo) ? "log" : "error"](msg);
  },

  /**
   * Reports a test result. The argument is an object with the following
   * properties:
   *
   * o message (string): message to be reported
   * o result (boolean): whether this test failed
   * o todo (boolean): whether this test is expected to fail
   */
  "report": function(test) {
    this.tests.push(test);
    this._log(test);
  },

  /**
   * Returns true if this test is expected to fail, and false otherwise.
   */
  "_todo": function(test) {
    if (this.expectedFailures === "all") {
      return true;
    }
    var value = this.expectedFailures[test.name];
    return value === true || (value === "debug" && !!SpecialPowers.isDebugBuild);
  },

  /**
   * Callback function for testharness.js. Called when one test in a file
   * finishes.
   */
  "result": function(test) {
    var url = this.getURL();
    this.report({
      "message": test.name + (test.message ? "; " + test.message : ""),
      "result": test.status === test.PASS,
      "todo": this._todo(test)
    });
  },

  /**
   * Callback function for testharness.js. Called when the entire test file
   * finishes.
   */
  "finish": function(tests, status) {
    var url = this.getURL();
    this.report({
      "message": "Finished test, status " + status.status,
      "result": status.status === status.OK,
      "todo":
        url in this.expectedFailures &&
        this.expectedFailures[url] === "error"
    });
    this.runner.testFinished(this.tests);
  }
};
(function() {
  if (!W3CTest.runner) {
    return;
  }
  // Get expected fails.  If there aren't any, there will be a 404, which is
  // fine.  Anything else is unexpected.
  var request = new XMLHttpRequest();
  request.open("GET", "/tests/dom/imptests/failures/" + W3CTest.getURL() + ".json", false);
  request.send();
  if (request.status === 200) {
    W3CTest.expectedFailures = JSON.parse(request.responseText);
  } else if (request.status !== 404) {
    is(request.status, 404, "Request status neither 200 nor 404");
  }

  add_result_callback(W3CTest.result.bind(W3CTest));
  add_completion_callback(W3CTest.finish.bind(W3CTest));
  setup({
    "output": false,
    "timeout": 1000000,
  });
})();
