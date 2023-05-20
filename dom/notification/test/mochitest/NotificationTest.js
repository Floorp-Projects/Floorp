var NotificationTest = (function () {
  "use strict";

  function info(msg, name) {
    SimpleTest.info("::Notification Tests::" + (name || ""), msg);
  }

  function setup_testing_env() {
    SimpleTest.waitForExplicitFinish();
    // turn on testing pref (used by notification.cpp, and mock the alerts
    return SpecialPowers.setBoolPref("notification.prompt.testing", true);
  }

  async function teardown_testing_env() {
    await SpecialPowers.clearUserPref("notification.prompt.testing");
    await SpecialPowers.clearUserPref("notification.prompt.testing.allow");

    SimpleTest.finish();
  }

  function executeTests(tests, callback) {
    // context is `this` object in test functions
    // it can be used to track data between tests
    var context = {};

    (function executeRemainingTests(remainingTests) {
      if (!remainingTests.length) {
        callback();
        return;
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
    run(tests, callback) {
      let ready = setup_testing_env();

      addLoadEvent(async function () {
        await ready;
        executeTests(tests, function () {
          teardown_testing_env();
          callback && callback();
        });
      });
    },

    allowNotifications() {
      return SpecialPowers.setBoolPref(
        "notification.prompt.testing.allow",
        true
      );
    },

    denyNotifications() {
      return SpecialPowers.setBoolPref(
        "notification.prompt.testing.allow",
        false
      );
    },

    clickNotification(notification) {
      // TODO: how??
    },

    fireCloseEvent(title) {
      window.dispatchEvent(
        new CustomEvent("mock-notification-close-event", {
          detail: {
            title,
          },
        })
      );
    },

    info,

    payload: {
      body: "Body",
      tag: "fakeTag",
      icon: "icon.jpg",
      lang: "en-US",
      dir: "ltr",
    },
  };
})();
