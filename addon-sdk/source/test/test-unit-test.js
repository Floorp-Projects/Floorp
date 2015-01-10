/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const timer = require("sdk/timers");

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
    timer.setTimeout(_ => {
        test.assertEqual(false, teardownCalled, "teardown not called until done");
        test.done();
    }, 20);
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

  test.waitUntil(_ => succeed, "waitUntil pass")
      .then(test.done);

  timer.setTimeout(_ => {
    test.pass("succeed");
    succeed = true;
  }, 20);
}

exports.testWaitUntilEqual = function(test) {
  test.waitUntilDone();
  let succeed = false;

  test.waitUntilEqual("foo", _ => succeed ? "foo" : "bar",
                      "waitUntilEqual pass")
      .then(test.done);

  timer.setTimeout(_ => {
    test.pass("succeed");
    succeed = true;
  }, 20);
}

exports.testWaitUntilNotEqual = function(test) {
  test.waitUntilDone();
  let succeed = false;

  test.waitUntilNotEqual("foo", _ => succeed ? "bar" : "foo",
                         "waitUntilNotEqual pass")
      .then(test.done);

  timer.setTimeout(_ => {
    test.pass("succeed");
    succeed = true;
  }, 20);
}

exports.testWaitUntilMatches = function(test) {
  test.waitUntilDone();
  let succeed = false;

  test.waitUntilMatches(_ => succeed ? "foo" : "bar",
                        /foo/, "waitUntilEqual pass")
      .then(test.done);

  timer.setTimeout(_ => {
    test.pass("succeed");
    succeed = true;
  }, 20);
}

exports.testWaitUntilErrorInCallback = function(test) {
  test.waitUntilDone();
  test.expectFail(_ => {
    test.waitUntil(_ => { throw "oops"; }, "waitUntil pass")
        .then(test.done);
  });
}

exports.testWaitUntilTimeoutInCallback = function(test) {
  test.waitUntilDone();

  let expected = [];
  let message = 0;
  if (require("sdk/test/options").parseable) {
    expected.push(["print", "TEST-START | wait4ever\n"]);
    expected.push(["error", "fail:", "Timed out (after: START)"]);
    expected.push(["error", "test assertion never became true:\n", "assertion failed, value is false\n"]);
    expected.push(["print", "TEST-END | wait4ever\n"]);
  }
  else {
    expected.push(["info",  "executing 'wait4ever'"]);
    expected.push(["error", "fail:", "Timed out (after: START)"]);
    expected.push(["error", "test assertion never became true:\n", "assertion failed, value is false\n"]);
  }

  function checkExpected(name, args) {
    if (expected.length == 0 || expected[0][0] != name) {
      test.fail("Saw an unexpected console." + name + "() call " + args);
      return;
    }

    message++;
    let expectedArgs = expected.shift().slice(1);
    for (let i = 0; i < expectedArgs.length; i++)
      test.assertEqual(args[i], expectedArgs[i], "Should have seen the right message in argument " + i + " of message " + message);
    if (expected.length == 0)
      test.done();
  }

  let runner = new (require("sdk/deprecated/unit-test").TestRunner)({
    console: {
      error: function() {
        checkExpected("error", Array.slice(arguments));
      },
      info: function () {
        checkExpected("info", Array.slice(arguments));
      },
      trace: function () {},
      exception: function () {},
      print: function () {
        checkExpected("print", Array.slice(arguments));
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

