var NotificationTest = (function () {
  "use strict";

  function info(msg, name) {
    SimpleTest.info("::Notification Tests::" + (name || ""), msg);
  }

  function setup_testing_env() {
    SimpleTest.waitForExplicitFinish();
    // turn on testing pref (used by notification.cpp, and mock the alerts
    SpecialPowers.setBoolPref("notification.prompt.testing", true);
  }

  function teardown_testing_env() {
    SimpleTest.finish();
  }

  function executeTests(tests, callback) {
    // context is `this` object in test functions
    // it can be used to track data between tests
    var context = {};

    (function executeRemainingTests(remainingTests) {
      if (!remainingTests.length) {
        return callback();
      }

      var nextTest = remainingTests.shift();
      var finishTest = executeRemainingTests.bind(null, remainingTests);
      var startTest = nextTest.call.bind(nextTest, context, finishTest);

      try {
        startTest();
        // if no callback was defined for test function,
        // we must manually invoke finish to continue
        if (nextTest.length === 0) {
          finishTest();
        }
      } catch (e) {
        ok(false, "Test threw exception!");
        finishTest();
      }
    })(tests);
  }

  // NotificationTest API
  return {
    run: function (tests, callback) {
      setup_testing_env();

      addLoadEvent(function () {
        executeTests(tests, function () {
          teardown_testing_env();
          callback && callback();
        });
      });
    },

    allowNotifications: function () {
      SpecialPowers.setBoolPref("notification.prompt.testing.allow", true);
    },

    denyNotifications: function () {
      SpecialPowers.setBoolPref("notification.prompt.testing.allow", false);
    },

    clickNotification: function (notification) {
      // TODO: how??
    },

    info: info
  };
})();
