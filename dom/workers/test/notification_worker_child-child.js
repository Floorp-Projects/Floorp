/* eslint-disable mozilla/no-comparison-or-assignment-inside-ok */

function ok(test, message) {
  postMessage({ type: "ok", test, message });
}

function is(a, b, message) {
  postMessage({ type: "is", test1: a, test2: b, message });
}

if (self.Notification) {
  var steps = [
    function () {
      ok(typeof Notification === "function", "Notification constructor exists");
      ok(Notification.permission, "Notification.permission exists");
      ok(
        typeof Notification.requestPermission === "undefined",
        "Notification.requestPermission should not exist"
      );
      //ok(typeof Notification.get === "function", "Notification.get exists");
    },

    function (done) {
      var options = {
        dir: "auto",
        lang: "",
        body: "This is a notification body",
        tag: "sometag",
        icon: "icon.png",
      };
      var notification = new Notification("This is a title", options);

      ok(notification !== undefined, "Notification exists");
      is(notification.onclick, null, "onclick() should be null");
      is(notification.onshow, null, "onshow() should be null");
      is(notification.onerror, null, "onerror() should be null");
      is(notification.onclose, null, "onclose() should be null");
      is(typeof notification.close, "function", "close() should exist");

      is(notification.dir, options.dir, "auto should get set");
      is(notification.lang, options.lang, "lang should get set");
      is(notification.body, options.body, "body should get set");
      is(notification.tag, options.tag, "tag should get set");
      is(notification.icon, options.icon, "icon should get set");

      // store notification in test context
      this.notification = notification;

      notification.onshow = function () {
        ok(true, "onshow handler should be called");
        done();
      };
    },

    function (done) {
      var notification = this.notification;

      notification.onclose = function () {
        ok(true, "onclose handler should be called");
        done();
      };

      notification.close();
    },
  ];

  onmessage = function (e) {
    var context = {};
    (function executeRemainingTests(remainingTests) {
      if (!remainingTests.length) {
        postMessage({ type: "finish" });
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
      } catch (ex) {
        ok(false, "Test threw exception! " + nextTest + " " + ex);
        finishTest();
      }
    })(steps);
  };
} else {
  ok(true, "Notifications are not enabled in workers on the platform.");
  postMessage({ type: "finish" });
}
