/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onclose = function() {
  postMessage("closed");
  // Try to open a new worker.
  try {
    var worker = new Worker("close_worker.js");
    throw new Error("We shouldn't get here!");
  } catch (e) {
    // pass
  }
};

setTimeout(function () {
  setTimeout(function () {
    throw new Error("I should never run!");
  }, 1000);
  close();
}, 1000);
