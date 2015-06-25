function info(message) {
  dump("INFO: " + message + "\n");
}

function ok(test, message) {
  postMessage({ type: 'ok', test: test, message: message });
}

function is(a, b, message) {
  postMessage({ type: 'is', test1: a, test2: b, message: message });
}

if (self.Notification) {
  var steps = [
    function (done) {
      info("Test notification permission");
      ok(typeof Notification === "function", "Notification constructor exists");
      ok(Notification.permission === "denied", "Notification.permission is denied.");

      var n = new Notification("Hello");
      n.onerror = function(e) {
        ok(true, "error called due to permission denied.");
        done();
      }
    },
  ];

  onmessage = function(e) {
    var context = {};
    (function executeRemainingTests(remainingTests) {
      if (!remainingTests.length) {
        postMessage({type: 'finish'});
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
        ok(false, "Test threw exception! " + nextTest + " " + e);
        finishTest();
      }
    })(steps);
  }
} else {
  ok(true, "Notifications are not enabled in workers on the platform.");
  postMessage({ type: 'finish' });
}
