function run_test() {
  do_load_manifest("data/chrome.manifest");
  do_test_pending();
  let mainThreadLocale = Intl.NumberFormat().resolvedOptions().locale;
  let testWorker = new Worker("chrome://locale/content/bug1174386_worker.js");
  testWorker.onmessage = function (e) {
    let workerLocale = e.data;
    equal(mainThreadLocale, workerLocale, "Worker should inherit Intl locale from main thread.");
    do_test_finished();
  };
  testWorker.postMessage("go!");
}
