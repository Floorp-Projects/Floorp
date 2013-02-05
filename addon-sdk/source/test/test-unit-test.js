/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const timer = require("sdk/timers");
const { Loader } = require("sdk/test/loader");

var setupCalled = false, teardownCalled = false;

exports.setup = function() {
    setupCalled = true;    
};

exports.teardown = function() {
    teardownCalled = true;
    setupCalled = false;
};

// Important note - unit tests are run in alphabetical order.  The following 
// unit tests for setup/teardown are order dependent, sometimes the result of 
// one test is checked in the next test (testing for teardown does this).  When 
// tests are cohesively a single unit, they are named <test_name> - partN where 
// N is their order in the sequence.  Secondly, because these tests should be
// run before all others, they start with an A.
exports.testASetupTeardownSyncTestPart1 = function(test) {
    test.assertEqual(true, setupCalled, 'setup function was called before this');
    test.assertEqual(false, teardownCalled, 'teardown function was not called before this');
};

exports.testASetupTeardownSyncTestPart2 = function(test) {
    test.assertEqual(true, setupCalled, 'setup was re-called before this');
    test.assertEqual(true, teardownCalled, 'teardown was called after first function');
};

exports.testATeardownAsyncTestPart1 = function(test) {
    teardownCalled = false;

    timer.setTimeout(function() {
        test.assertEqual(false, teardownCalled, "teardown not called until done");
        test.done();
    }, 200);
    test.waitUntilDone();
};

exports.testATeardownAsyncTestPart2 = function(test) {
    test.assertEqual(true, teardownCalled, "teardown called after done");
};

exports.testWaitUntilInstant = function(test) {
  test.waitUntilDone();
  
  test.waitUntil(function () true, "waitUntil with instant true pass")
      .then(function () test.done());
}

exports.testWaitUntil = function(test) {
  test.waitUntilDone();
  let succeed = false;
  
  test.waitUntil(function () succeed, "waitUntil pass")
      .then(function () test.done());
  
  timer.setTimeout(function () {
    succeed = true;
  }, 200);
}

exports.testWaitUntilEqual = function(test) {
  test.waitUntilDone();
  let succeed = false;
  
  test.waitUntilEqual("foo", function () succeed ? "foo" : "bar", 
                      "waitUntilEqual pass")
      .then(function () test.done());
  
  timer.setTimeout(function () {
    succeed = true;
  }, 200);
}

exports.testWaitUntilNotEqual = function(test) {
  test.waitUntilDone();
  let succeed = false;
  
  test.waitUntilNotEqual("foo", function () succeed ? "bar" : "foo",
                         "waitUntilNotEqual pass")
      .then(function () test.done());
  
  timer.setTimeout(function () {
    succeed = true;
  }, 200);
}

exports.testWaitUntilMatches = function(test) {
  test.waitUntilDone();
  let succeed = false;
  
  test.waitUntilMatches(function () succeed ? "foo" : "bar",
                        /foo/, "waitUntilEqual pass")
      .then(function () test.done());
  
  timer.setTimeout(function () {
    succeed = true;
  }, 200);
}

exports.testWaitUntilErrorInCallback = function(test) {
  test.waitUntilDone();

  test.expectFail(function() {
    test.waitUntil(function () {throw "oops"}, "waitUntil pass")
        .then(function () test.done());
  });
}

exports.testWaitUntilTimeoutInCallback = function(test) {
  test.waitUntilDone(1000);

  let runner = new (require("sdk/deprecated/unit-test").TestRunner)({
    console: {
      calls: 0,
      error: function(msg) {
        this.calls++;
        if (this.calls == 2) {
          test.assertEqual(arguments[0], "test assertion never became true:\n");
          test.assertEqual(arguments[1], "assertion failed, value is false\n");
          // We could additionally check that arguments[1] contains the correct
          // stack, but it would be difficult to do so given that it contains
          // resource: URLs with a randomly generated string embedded in them
          // (the ID of the test addon created to run the tests). And in any
          // case, checking the arguments seems sufficient.

          test.done();
        }
        else {
          test.fail("We got unexpected console.error() calls from waitUntil" +
                    " assertion callback: '" + arguments[1] + "'");
        }
      },
      info: function (msg) {
        this.calls++;
        if (require("@test/options").parseable) {
          test.fail("We got unexpected console.info() calls: " + msg)
        }
        else if (this.calls == 1) {
          test.assertEqual(arguments[0], "executing 'wait4ever'");
        }
        else {
          test.fail("We got unexpected console.info() calls: " + msg);
        }
      },
      trace: function () {},
      exception: function () {},
      print: function (str) {
        this.calls++;
        if (!require("@test/options").parseable) {
          test.fail("We got unexpected console.print() calls: " + str)
        }
        else if (this.calls == 1) {
          test.assertEqual(str, "TEST-START | wait4ever\n");
        }
        else if (this.calls == 3) {
          test.assertEqual(str, "TEST-END | wait4ever\n");
        }
        else {
          test.fail("We got unexpected console.print() calls: " + str);
        }
      }
    }
  });

  runner.start({
    test: {
      name: "wait4ever",
      testFunction: function(test) {
        test.waitUntilDone(100);
        test.waitUntil(function() false);
      }
    },
    onDone: function() {}
  });
};

exports.testExpectFail = function(test) {
    test.expectFail(function() {
        test.fail('expectFail masking .fail');
    });

    test.expectFail(function() {
        test.assert(false, 'expectFail masking .assert');
    });

    test.assert(true, 'assert should pass with no expectFail');
/*
    test.expectFail(function() {
        test.expectFail(function() {
            test.fail('this should blow up');
        });
    });
*/
};

exports.testAssertFunction = function(test) {
    test.assertFunction(function() {}, 'assertFunction with function');
    test.expectFail(function() {
        test.assertFunction(null, 'assertFunction with non-function');
    });    
};

exports.testAssertUndefined = function(test) {
    test.assertUndefined(undefined, 'assertUndefined with undefined');
    test.expectFail(function() {
        test.assertUndefined(null, 'assertUndefined with null');
    });    
    test.expectFail(function() {
        test.assertUndefined(false, 'assertUndefined with false');
    });    
    test.expectFail(function() {
        test.assertUndefined(0, 'assertUndefined with 0');
    });    
};

exports.testAssertNotUndefined = function(test) {
    test.expectFail(function() {
        test.assertNotUndefined(undefined, 'assertNotUndefined with undefined');
    });    
    test.assertNotUndefined(null, 'assertNotUndefined with null');
    test.assertNotUndefined(false, 'assertNotUndefined with false');
    test.assertNotUndefined(0, 'assertNotUndefined with 0');
};

exports.testAssertNull = function(test) {
    test.assertNull(null, 'assertNull with null');
    test.expectFail(function() {
        test.assertNull(undefined, 'assertNull with undefined');
    });    
    test.expectFail(function() {
        test.assertNull(false, 'assertNull with false');
    });
    test.expectFail(function() {
        test.assertNull(0, 'assertNull with 0');
    });
};

exports.testAssertNotNull = function(test) {
    test.assertNotNull(undefined, 'assertNotNull with undefined');
    test.assertNotNull(false, 'assertNotNull with false');
    test.assertNotNull(0, 'assertNotNull with 0');

    test.expectFail(function() {
        test.assertNotNull(null, 'testAssertNotNull with null');
    });    
};

exports.testAssertObject = function(test) {
    test.assertObject({}, 'assertObject with {}' );
    test.assertObject(new Object(), 'assertObject with new Object');
    test.expectFail(function() {
        test.assertObject('fail', 'assertObject with string');
    });
};

exports.testAssertString = function(test) {
    test.assertString('', 'assertString with ""');
    test.assertString(new String(), 'assertString with new String');
};

exports.testAssertArray = function(test) {
    test.assertArray([], 'assertArray with []');
    test.assertArray(new Array(), 'assertArray with new Array');
};

exports.testNumber = function(test) {
    test.assertNumber(1, 'assertNumber with 1');
    test.assertNumber(new Number('2'), 'assertNumber with new Number("2")' );
};

