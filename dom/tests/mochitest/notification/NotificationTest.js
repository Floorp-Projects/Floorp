var NotificationTest = (function () {
  "use strict";

  function info(msg, name) {
    SimpleTest.info("::Notification Tests::" + (name || ""), msg);
  }

  function setup_testing_env() {
    SimpleTest.waitForExplicitFinish();
    // turn on testing pref (used by notification.cpp, and mock the alerts
    SpecialPowers.setBoolPref("notification.prompt.testing", true);
    SpecialPowers.setAllAppsLaunchable(true);
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

  function fakeApp(aManifest) {
    var aApp = {
      "origin": "{mochitest}",
      "manifestURL": aManifest
    };

    SpecialPowers.injectApp("{mochitest}", aApp);
  }

  function unfakeApp() {
    SpecialPowers.rejectApp("{mochitest}");
  }

  var fakeCustomData = (function () {
    var buffer = new ArrayBuffer(2);
    var dv = new DataView(buffer).setInt16(0, 42, true);
    var canvas = document.createElement("canvas");
    canvas.width = canvas.height = 100;
    var context = canvas.getContext("2d");

    var map = new Map();
    var set = new Set();
    map.set("test", 42);
    set.add(4); set.add(2);

    return {
      primitives: {
        a: 123,
        b: "test",
        c: true,
        d: [1, 2, 3]
      },
      date: new Date(2013, 2, 1, 1, 10),
      regexp: new RegExp("[^.]+"),
      arrayBuffer: buffer,
      imageData: context.createImageData(100, 100),
      map: map,
      set: set
    };
  })();

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

    fireCloseEvent: function (title) {
      window.dispatchEvent(new CustomEvent("mock-notification-close-event", {
        detail: {
          title: title
        }
      }));
    },

    info: info,

    fakeApp: fakeApp,

    unfakeApp: unfakeApp,

    customDataMatches: function(dataObj) {
      var url = "http://www.domain.com";
      try {
        return (JSON.stringify(dataObj.primitives) ===
                JSON.stringify(fakeCustomData.primitives)) &&
               (dataObj.date.toDateString() ===
                fakeCustomData.date.toDateString()) &&
               (dataObj.regexp.exec(url)[0].substr(7) === "www") &&
               (new Int16Array(dataObj.arrayBuffer)[0] === 42) &&
               (JSON.stringify(dataObj.imageData.data) ===
                JSON.stringify(fakeCustomData.imageData.data)) &&
               (dataObj.map.get("test") == 42) &&
               (dataObj.set.has(4) && dataObj.set.has(2));
      } catch(e) {
        return false;
      }
    },

    payload: {
      body: "Body",
      tag: "fakeTag",
      icon: "icon.jpg",
      lang: "en-US",
      dir: "ltr",
      data: fakeCustomData
    }
  };
})();
