/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

if (self.name != "FrameWorker") {
  throw new Error("Bad worker name: " + self.name);
}

var registeredPorts = [];
var errorCount = 0;
var storedData;

self.onconnect = function (event) {
  var port = event.ports[0];

  if (registeredPorts.length) {
    let data = {
      type: "connect",
    };

    registeredPorts.forEach(function (registeredPort) {
      registeredPort.postMessage(data);
    });
  }

  port.onmessage = function (msg) {
    switch (msg.data.command) {
      case "start":
        break;

      case "error":
        throw new Error("Expected");

      case "store":
        storedData = msg.data.data;
        break;

      case "retrieve":
        var data = {
          type: "result",
          data: storedData,
        };
        port.postMessage(data);
        break;

      default:
        throw new Error("Unknown command '" + error.data.command + "'");
    }
  };

  registeredPorts.push(port);
};

self.onerror = function (message, filename, lineno) {
  if (!errorCount++) {
    var data = {
      type: "worker-error",
      message,
      filename,
      lineno,
    };

    registeredPorts.forEach(function (registeredPort) {
      registeredPort.postMessage(data);
    });

    // Prevent the error from propagating the first time only.
    return true;
  }
  return undefined;
};
