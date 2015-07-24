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

// Make sure that we can't access indexedDB
try {
  indexedDB;
  ok(false, "WORKER getting indexedDB should have thrown");
} catch (e) {
  ok(true, "WORKER getting indexedDB threw");
}

// Make sure that we can't access caches
try {
  var promise = caches.keys();
  ok(true, "WORKER getting caches didn't throw");

  promise.then(function() {
    ok(false, "WORKER The promise should have rejected");
    workerTest();
  }, function() {
    ok(true, "WORKER The promise was rejected");
    workerTest();
  });
} catch (e) {
  ok(false, "WORKER getting caches should not have thrown");
}

// Try to spawn an inner worker, and make sure that it also can't access storage
function workerTest() {
  if (location.hash == "#inner") { // Don't recurse infinitely, if we are the inner worker, don't spawn another
    finishTest();
    return;
  }
  // Create the inner worker, and listen for test messages from it
  var worker = new Worker("workerStoragePrevented.js#inner");
  worker.addEventListener('message', function(e) {
    if (e.data == "done") {
      finishTest();
      return;
    }

    ok(!e.data.match(/^FAILURE/), e.data + " (WORKER = workerStoragePrevented.js#inner)");
  });
}
