/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
   * If set to true, we will dump the test failures to the console.
   */
  "dumpFailures": false,

  /**
   * If dumpFailures is true, this holds a structure like necessary for
   * expectedFailures, for ease of updating the expectations.
   */
  "failures": {},

  /**
   * List of test results, needed by TestRunner to update the UI.
   */
  "tests": [],

  /**
   * Number of unlogged passes, to stop buildbot from truncating the log.
   * We will print a message every MAX_COLLAPSED_MESSAGES passes.
   */
  "collapsedMessages": 0,
  "MAX_COLLAPSED_MESSAGES": 100,

  /**
   * Reference to the TestRunner object in the parent frame.
   */
  "runner": parent === this ? null : parent.TestRunner || parent.wrappedJSObject.TestRunner,

  /**
   * Prefixes for the error logging. Indexed first by int(todo) and second by
   * int(result). Also contains the test's status, and expected status.
   */
  "prefixes": [
    [
      {status: 'FAIL', expected: 'PASS', message: "TEST-UNEXPECTED-FAIL"},
      {status: 'PASS', expected: 'PASS', message: "TEST-PASS"}
    ],
    [
      {status: 'FAIL', expected: 'FAIL', message: "TEST-KNOWN-FAIL"},
      {status: 'PASS', expected: 'FAIL', message: "TEST-UNEXPECTED-PASS"}
    ]
  ],

  /**
   * Prefix of the path to parent of the the failures directory.
   */
  "pathprefix": "/tests/dom/imptests/",

  /**
   * Returns the URL of the current test, relative to the root W3C tests
   * directory. Used as a key into the expectedFailures dictionary.
   */
  "getPath": function() {
    var url = this.getURL();
    if (!url.startsWith(this.pathprefix)) {
      return "";
    }
    return url.substring(this.pathprefix.length);
  },

  /**
   * Returns the root-relative URL of the current test.
   */
  "getURL": function() {
    return this.runner ? this.runner.currentTestURL : location.pathname;
  },

  /**
   * Report the results in the tests array.
   */
  "reportResults": function() {
    var element = function element(aLocalName) {
      var xhtmlNS = "http://www.w3.org/1999/xhtml";
      return document.createElementNS(xhtmlNS, aLocalName);
    };

    var stylesheet = element("link");
    stylesheet.setAttribute("rel", "stylesheet");
    stylesheet.setAttribute("href", "/resources/testharness.css");
    var heads = document.getElementsByTagName("head");
    if (heads.length) {
      heads[0].appendChild(stylesheet);
    }

    var log = document.getElementById("log");
    if (!log) {
      return;
    }
    var section = log.appendChild(element("section"));
    section.id = "summary";
    section.appendChild(element("h2")).textContent = "Details";

    var table = section.appendChild(element("table"));
    table.id = "results";

    var tr = table.appendChild(element("thead")).appendChild(element("tr"));
    for (var header of ["Result", "Test Name", "Message"]) {
      tr.appendChild(element("th")).textContent = header;
    }
    var statuses = [
      ["Unexpected Fail", "Pass"],
      ["Known Fail", "Unexpected Pass"]
    ];
    var tbody = table.appendChild(element("tbody"));
    for (var test of this.tests) {
      tr = tbody.appendChild(element("tr"));
      tr.className = (test.result === !test.todo ? "pass" : "fail");
      tr.appendChild(element("td")).textContent =
        statuses[+test.todo][+test.result];
      tr.appendChild(element("td")).textContent = test.name;
      tr.appendChild(element("td")).textContent = test.message;
    }
  },

  /**
   * Returns a printable message based on aTest's 'name' and 'message'
   * properties.
   */
  "formatTestMessage": function(aTest) {
    return aTest.name + (aTest.message ? ": " + aTest.message : "");
  },

  /**
   * Lets the test runner know about a test result.
   */
  "_log": function(test) {
    var url = this.getURL();
    var message = this.formatTestMessage(test);
    var result = this.prefixes[+test.todo][+test.result];

    if (this.runner) {
      this.runner.structuredLogger.testStatus(url,
                                              test.name,
                                              result.status,
                                              result.expected,
                                              message);
    } else {
      var msg = result.message + " | ";
      if (url) {
        msg += url;
      }
      msg += " | " + this.formatTestMessage(test);
      dump(msg + "\n");
    }
  },

  /**
   * Logs a message about collapsed messages (if any), and resets the counter.
   */
  "_logCollapsedMessages": function() {
    if (this.collapsedMessages) {
      this._log({
        "name": document.title,
        "result": true,
        "todo": false,
        "message": "Elided " + this.collapsedMessages + " passes or known failures."
      });
    }
    this.collapsedMessages = 0;
  },

  /**
   * Maybe logs a result, eliding up to MAX_COLLAPSED_MESSAGES consecutive
   * passes.
   */
  "_maybeLog": function(test) {
    var success = (test.result === !test.todo);
    if (success && ++this.collapsedMessages < this.MAX_COLLAPSED_MESSAGES) {
      return;
    }
    this._logCollapsedMessages();
    this._log(test);
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
    this._maybeLog(test);
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
    var url = this.getPath();
    this.report({
      "name": test.name,
      "message": test.message || "",
      "result": test.status === test.PASS,
      "todo": this._todo(test)
    });
    if (this.dumpFailures && test.status !== test.PASS) {
      this.failures[test.name] = true;
    }
  },

  /**
   * Callback function for testharness.js. Called when the entire test file
   * finishes.
   */
  "finish": function(tests, status) {
    var url = this.getPath();
    this.report({
      "name": "Finished test",
      "message": "Status: " + status.status,
      "result": status.status === status.OK,
      "todo":
        url in this.expectedFailures &&
        this.expectedFailures[url] === "error"
    });

    this._logCollapsedMessages();

    if (this.dumpFailures) {
      dump("@@@ @@@ Failures\n");
      dump(url + "@@@" + JSON.stringify(this.failures) + "\n");
    }
    if (this.runner) {
      this.runner.testFinished(this.tests.map(function(aTest) {
        return {
          "message": this.formatTestMessage(aTest),
          "result": aTest.result,
          "todo": aTest.todo
        };
      }, this));
    } else {
      this.reportResults();
    }
  },

  /**
   * Log an unexpected failure. Intended to be used from harness code, not
   * from tests.
   */
  "logFailure": function(aTestName, aMessage) {
    this.report({
      "name": aTestName,
      "message": aMessage,
      "result": false,
      "todo": false
    });
  },

  /**
   * Timeout the current test. Intended to be used from harness code, not
   * from tests.
   */
  "timeout": function() {
    this.logFailure("Timeout", "Test runner timed us out.");
    timeout();
  }
};
(function() {
  try {
    var path = W3CTest.getPath();
    if (path) {
      // Get expected fails.  If there aren't any, there will be a 404, which is
      // fine.  Anything else is unexpected.
      var url = W3CTest.pathprefix + "failures/" + path + ".json";
      var request = new XMLHttpRequest();
      request.open("GET", url, false);
      request.send();
      if (request.status === 200) {
        W3CTest.expectedFailures = JSON.parse(request.responseText);
      } else if (request.status !== 404) {
        W3CTest.logFailure("Fetching failures file", "Request status was " + request.status);
      }
    }

    add_result_callback(W3CTest.result.bind(W3CTest));
    add_completion_callback(W3CTest.finish.bind(W3CTest));
    setup({
      "output": W3CTest.runner && !W3CTest.runner.getParameterInfo().closeWhenDone,
      "explicit_timeout": true
    });
  } catch (e) {
    W3CTest.logFailure("Harness setup", "Unexpected exception: " + e);
  }
})();
