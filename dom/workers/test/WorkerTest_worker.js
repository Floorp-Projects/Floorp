/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onmessage = function(event) {
  let worker = new ChromeWorker("WorkerTest_subworker.js");
  worker.onmessage = function(event) {
    postMessage(event.data);
  }
  worker.postMessage(event.data);
}
