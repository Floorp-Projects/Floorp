var query = window.location.search;
var bc = new BroadcastChannel("bugSharedWorkerLiftetime" + query);
bc.onmessage = msgEvent => {
  var msg = msgEvent.data;
  var command = msg.command;
  if (command == "postToWorker") {
    postToWorker(msg.workerMessage);
  } else if (command == "navigate") {
    window.location = msg.location;
  } else if (command == "finish") {
    bc.postMessage({ command: "finished" });
    bc.close();
    window.close();
  }
};

window.onload = () => {
  bc.postMessage({ command: "loaded" });
};

function debug(message) {
  if (typeof message != "string") {
    throw new Error("debug() only accepts strings!");
  }
  bc.postMessage({ command: "debug", message });
}

let worker;

function postToWorker(msg) {
  if (!worker) {
    worker = new SharedWorker(
      "multi_sharedWorker_sharedWorker.js",
      "FrameWorker"
    );
    worker.onerror = function (error) {
      debug("Worker error: " + error.message);
      error.preventDefault();

      let data = {
        type: "error",
        message: error.message,
        filename: error.filename,
        lineno: error.lineno,
        isErrorEvent: error instanceof ErrorEvent,
      };
      bc.postMessage({ command: "fromWorker", workerMessage: data });
    };

    worker.port.onmessage = function (message) {
      debug("Worker message: " + JSON.stringify(message.data));
      bc.postMessage({ command: "fromWorker", workerMessage: message.data });
    };
  }

  debug("Posting message: " + JSON.stringify(msg));
  worker.port.postMessage(msg);
}
