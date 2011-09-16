/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
if (!("ctypes" in self)) {
  throw "No ctypes!";
}

// Go ahead and verify that the ctypes lazy getter actually works.
if (ctypes.toString() != "[object ctypes]") {
  throw "Bad ctypes object: " + ctypes.toString();
}

onmessage = function(event) {
  let worker = new ChromeWorker("chromeWorker_subworker.js");
  worker.onmessage = function(event) {
    postMessage(event.data);
  }
  worker.postMessage(event.data);
}
