var NotificationTest = (function() {
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

  var fakeCustomData = (function() {
    var buffer = new ArrayBuffer(2);
    new DataView(buffer).setInt16(0, 42, true);
    var canvas = document.createElement("canvas");
    canvas.width = canvas.height = 100;
    var context = canvas.getContext("2d");

    var map = new Map();
    var set = new Set();
    map.set("test", 42);
    set.add(4);
    set.add(2);

    return {
      primitives: {
        a: 123,
        b: "test",
        c: true,
        d: [1, 2, 3],
      },
      date: new Date(2013, 2, 1, 1, 10),
      regexp: new RegExp("[^.]+"),
      arrayBuffer: buffer,
      imageData: context.createImageData(100, 100),
      map,
      set,
    };
  })();

  // NotificationTest API
  return {
    run(tests, callback) {
      let ready = setup_testing_env();

      addLoadEvent(async function() {
        await ready;
        executeTests(tests, function() {
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

    customDataMatches(dataObj) {
      var url = "http://www.domain.com";
      try {
        return (
          JSON.stringify(dataObj.primitives) ===
            JSON.stringify(fakeCustomData.primitives) &&
          dataObj.date.toDateString() === fakeCustomData.date.toDateString() &&
          dataObj.regexp.exec(url)[0].substr(7) === "www" &&
          new Int16Array(dataObj.arrayBuffer)[0] === 42 &&
          JSON.stringify(dataObj.imageData.data) ===
            JSON.stringify(fakeCustomData.imageData.data) &&
          dataObj.map.get("test") == 42 &&
          (dataObj.set.has(4) && dataObj.set.has(2))
        );
      } catch (e) {
        return false;
      }
    },

    payload: {
      body: "Body",
      tag: "fakeTag",
      icon: "icon.jpg",
      lang: "en-US",
      dir: "ltr",
      data: fakeCustomData,
    },
  };
})();
