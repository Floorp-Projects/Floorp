"use strict";

function f() {
  debugger;
}

var worker = new Worker("WorkerDebuggerGlobalScope.enterEventLoop_childWorker.js");

worker.onmessage = function (event) {
  postMessage("child:" + event.data);
};

self.onmessage = function (event) {
  var message = event.data;
  if (message.indexOf(":") >= 0) {
    worker.postMessage(message.split(":")[1]);
    return;
  }
  switch (message) {
  case "ping":
    debugger;
    postMessage("pong");
    break;
  };
};
