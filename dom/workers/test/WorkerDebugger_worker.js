"use strict";

var worker = new Worker("WorkerDebugger_childWorker.js");
self.onmessage = function (event) {
  postMessage("child:" + event.data);
};
debugger;
postMessage("worker");
