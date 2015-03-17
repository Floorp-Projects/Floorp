"use strict";

var worker = new Worker("WorkerDebugger.initialize_childWorker.js");
worker.onmessage = function (event) {
  postMessage("child:" + event.data);
};

debugger;
postMessage("worker");
