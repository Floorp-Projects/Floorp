/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

if (!("self" in this)) {
  throw new Error("No 'self' exists on SharedWorkerGlobalScope!");
}
if (this !== self) {
  throw new Error("'self' not equal to global object!");
}
if (!(self instanceof SharedWorkerGlobalScope)) {
  throw new Error("self not a SharedWorkerGlobalScope instance!");
}

var propsToCheck = [
  "location",
  "navigator",
  "close",
  "importScripts",
  "setTimeout",
  "clearTimeout",
  "setInterval",
  "clearInterval",
  "dump",
  "atob",
  "btoa"
];

for (var index = 0; index < propsToCheck.length; index++) {
  var prop = propsToCheck[index];
  if (!(prop in self)) {
    throw new Error("SharedWorkerGlobalScope has no '" + prop + "' property!");
  }
}

onconnect = function(event) {
  if (!("SharedWorkerGlobalScope" in self)) {
    throw new Error("SharedWorkerGlobalScope should be visible!");
  }
  if (!(self instanceof SharedWorkerGlobalScope)) {
    throw new Error("The global should be a SharedWorkerGlobalScope!");
  }
  if (!(self instanceof WorkerGlobalScope)) {
    throw new Error("The global should be a WorkerGlobalScope!");
  }
  if ("DedicatedWorkerGlobalScope" in self) {
    throw new Error("DedicatedWorkerGlobalScope should not be visible!");
  }
  if (!(event instanceof MessageEvent)) {
    throw new Error("'connect' event is not a MessageEvent!");
  }
  if (!("ports" in event)) {
    throw new Error("'connect' event doesn't have a 'ports' property!");
  }
  if (event.ports.length != 1) {
    throw new Error("'connect' event has a 'ports' property with length '" +
                    event.ports.length + "'!");
  }
  if (!event.ports[0]) {
    throw new Error("'connect' event has a null 'ports[0]' property!");
  }
  if (!(event.ports[0] instanceof MessagePort)) {
    throw new Error("'connect' event has a 'ports[0]' property that isn't a " +
                    "MessagePort!");
  }
  if (!(event.ports[0] == event.source)) {
    throw new Error("'connect' event source property is incorrect!");
  }
  if (event.data) {
    throw new Error("'connect' event has data: " + event.data);
  }

  event.ports[0].onmessage = function(event) {
    if (!(event instanceof MessageEvent)) {
      throw new Error("'message' event is not a MessageEvent!");
    }
    if (!("ports" in event)) {
      throw new Error("'message' event doesn't have a 'ports' property!");
    }
    if (!(event.ports === null)) {
      throw new Error("'message' event has a non-null 'ports' property!");
    }
    event.target.postMessage(event.data);
    throw new Error(event.data);
  };
};
