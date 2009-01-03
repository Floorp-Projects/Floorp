const importSubURL = "relativeLoad_sub_import.js";

onmessage = function(event) {
  importScripts(importSubURL);
  var worker = new Worker("relativeLoad_sub_worker2.js");
  worker.onerror = function(event) {
    throw event.data;
  };
  worker.onmessage = function(event) {
    if (event.data != workerSubURL) {
      throw "Bad data!";
    }
    postMessage(workerSubURL);
  };
};
