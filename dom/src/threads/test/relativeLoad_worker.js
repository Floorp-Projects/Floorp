const importURL = "relativeLoad_import.js";

onmessage = function(event) {
  importScripts(importURL);
  var worker = new Worker("relativeLoad_worker2.js");
  worker.onerror = function(event) {
    throw event.data;
  };
  worker.onmessage = function(event) {
    if (event.data != workerURL) {
      throw "Bad data!";
    }
    postMessage(workerURL);
  }
};
