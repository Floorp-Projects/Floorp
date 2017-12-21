/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


// Worker must be loaded from a chrome:// uri, not a file://
// uri, so we first need to load it.
var WORKER_SOURCE_URI = "chrome://workers/content/worker.js";
do_load_manifest("data/chrome.manifest");

function run_test() {
  run_next_test();
}

function talk_with_worker(worker) {
  return new Promise((resolve, reject) => {
    worker.onmessage = function(event) {
      let success = true;
      if (event.data == "OK") {
        resolve();
      } else {
        success = false;
        reject(event);
      }
      Assert.ok(success);
      worker.terminate();
    };
    worker.onerror = function(event) {
      let error = new Error(event.message, event.filename, event.lineno);
      worker.terminate();
      reject(error);
    };
    worker.postMessage("START");
  });
}


add_task(function test_chrome_worker() {
  return talk_with_worker(new ChromeWorker(WORKER_SOURCE_URI));
});

add_task(function test_worker() {
  return talk_with_worker(new Worker(WORKER_SOURCE_URI));
});
