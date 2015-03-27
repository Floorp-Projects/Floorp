"use strict";

function f() {
  debugger;
}

self.onmessage = function (event) {
  switch (event.data) {
  case "ping":
    debugger;
    postMessage("pong");
    break;
  };
};
