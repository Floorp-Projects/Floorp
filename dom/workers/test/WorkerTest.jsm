/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var EXPORTED_SYMBOLS = ["WorkerTest"];

var WorkerTest = {
  go(message, messageCallback, errorCallback) {
    let worker = new ChromeWorker("WorkerTest_worker.js");
    worker.onmessage = messageCallback;
    worker.onerror = errorCallback;
    worker.postMessage(message);
    return worker;
  },
};
