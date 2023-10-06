"use strict";

function f() {
  // eslint-disable-next-line no-debugger
  debugger;
}

var worker = new Worker(
  "WorkerDebuggerGlobalScope.enterEventLoop_childWorker.js"
);

worker.onmessage = function (event) {
  postMessage("child:" + event.data);
};

self.onmessage = function (event) {
  var message = event.data;
  if (message.includes(":")) {
    worker.postMessage(message.split(":")[1]);
    return;
  }
  switch (message) {
    case "ping":
      // eslint-disable-next-line no-debugger
      debugger;
      postMessage("pong");
      break;
  }
};
