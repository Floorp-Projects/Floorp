// Unfortunately, workers can't share the code from storagePermissionsUtils.
// These are basic mechanisms for communicating to the test runner.

function ok(condition, text) {
  if (!condition) {
    self.postMessage("FAILURE: " + text);
  } else {
    self.postMessage(text);
  }
}

function finishTest() {
  self.postMessage("done");
  self.close();
}

// Workers don't have access to localstorage or sessionstorage
ok(typeof self.localStorage == "undefined", "localStorage should be undefined");
ok(typeof self.sessionStorage == "undefined", "sessionStorage should be undefined");

// Make sure that we can access indexedDB
try {
  indexedDB;
  ok(true, "WORKER getting indexedDB didn't throw");
} catch (e) {
  ok(false, "WORKER getting indexedDB should not throw");
}

// Make sure that we can access caches
try {
  var promise = caches.keys();
  ok(true, "WORKER getting caches didn't throw");

  promise.then(function() {
    ok(location.protocol == "https:", "WORKER The promise was not rejected");
    workerTest();
  }, function() {
    ok(location.protocol != "https:", "WORKER The promise should not have been rejected");
    workerTest();
  });
} catch (e) {
  ok(false, "WORKER getting caches should not have thrown");
}

// Try to spawn an inner worker, and make sure that it can also access storage
function workerTest() {
  if (location.hash == "#inner") { // Don't recurse infinitely, if we are the inner worker, don't spawn another
    finishTest();
    return;
  }
  // Create the inner worker, and listen for test messages from it
  var worker = new Worker("workerStorageAllowed.js#inner");
  worker.addEventListener('message', function(e) {
    if (e.data == "done") {
      finishTest();
      return;
    }

    ok(!e.data.match(/^FAILURE/), e.data + " (WORKER = workerStorageAllowed.js#inner)");
  });
}
