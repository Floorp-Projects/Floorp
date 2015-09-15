/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Promise.jsm");

// Worker must be loaded from a chrome:// uri, not a file://
// uri, so we first need to load it.
var WORKER_SOURCE_URI = "chrome://workers/content/worker.js";
do_load_manifest("data/chrome.manifest");

function run_test() {
  run_next_test();
}

function talk_with_worker(worker) {
  let deferred = Promise.defer();
  worker.onmessage = function(event) {
    let success = true;
    if (event.data == "OK") {
      deferred.resolve();
    } else {
      success = false;
      deferred.reject(event);
    }
    do_check_true(success);
    worker.terminate();
  };
  worker.onerror = function(event) {
    let error = new Error(event.message, event.filename, event.lineno);
    worker.terminate();
    deferred.reject(error);
  };
  worker.postMessage("START");
  return deferred.promise;
}


add_task(function test_chrome_worker() {
  return talk_with_worker(new ChromeWorker(WORKER_SOURCE_URI));
});

add_task(function test_worker() {
  return talk_with_worker(new Worker(WORKER_SOURCE_URI));
});
