/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
if (!("ctypes" in self)) {
  throw "No ctypes!";
}

onmessage = function(event) {
  let worker = new ChromeWorker("chromeWorker_subworker.js");
  worker.onmessage = function(event) {
    postMessage(event.data);
  }
  worker.postMessage(event.data);
}
