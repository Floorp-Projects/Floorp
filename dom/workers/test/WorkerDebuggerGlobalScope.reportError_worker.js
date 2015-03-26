"use strict";

var worker = new Worker("WorkerDebuggerGlobalScope.reportError_childWorker.js");

worker.onmessage = function (event) {
  postMessage("child:" + event.data);
};

self.onerror = function () {
  postMessage("error");
};
