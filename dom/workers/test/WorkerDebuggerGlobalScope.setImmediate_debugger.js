"use strict";

this.onmessage = function (event) {
  switch (event.data) {
  case "ping":
    setImmediate(function () {
      postMessage("pong1");
    });
    postMessage("pong2");
    break;
  }
};
