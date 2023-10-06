"use strict";

var worker = new Worker("WorkerDebugger.initialize_childWorker.js");
worker.onmessage = function (event) {
  postMessage("child:" + event.data);
};

// eslint-disable-next-line no-debugger
debugger;
postMessage("worker");
