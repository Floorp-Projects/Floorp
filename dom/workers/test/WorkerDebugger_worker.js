"use strict";

var worker = new Worker("WorkerDebugger_childWorker.js");
self.onmessage = function (event) {
  postMessage("child:" + event.data);
};
// eslint-disable-next-line no-debugger
debugger;
postMessage("worker");
