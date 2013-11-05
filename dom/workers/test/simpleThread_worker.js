/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

function messageListener(event) {
  var exception;
  try {
    event.bubbles = true;
  }
  catch(e) {
    exception = e;
  }

  if (!(exception instanceof TypeError)) {
    throw exception;
  }

  switch (event.data) {
    case "no-op":
      break;
    case "components":
      postMessage(Components.toString());
      break;
    case "start":
      for (var i = 0; i < 1000; i++) { }
      postMessage("started");
      break;
    case "stop":
      self.postMessage('no-op');
      postMessage("stopped");
      self.removeEventListener("message", messageListener, false);
      break;
    default:
      throw 'Bad message: ' + event.data;
  }
}

if (!("DedicatedWorkerGlobalScope" in self)) {
  throw new Error("DedicatedWorkerGlobalScope should be visible!");
}
if (!(self instanceof DedicatedWorkerGlobalScope)) {
  throw new Error("The global should be a SharedWorkerGlobalScope!");
}
if (!(self instanceof WorkerGlobalScope)) {
  throw new Error("The global should be a WorkerGlobalScope!");
}
if ("SharedWorkerGlobalScope" in self) {
  throw new Error("SharedWorkerGlobalScope should not be visible!");
}

addEventListener("message", { handleEvent: messageListener });
