const importSubURL = "relativeLoad_sub_import.js";

onmessage = function(event) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "testXHR.txt", false);
  xhr.send(null);
  if (xhr.status != 404) {
    throw "Loaded an xhr from the wrong location!";
  }

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
