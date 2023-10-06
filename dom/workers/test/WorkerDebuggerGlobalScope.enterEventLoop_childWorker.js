"use strict";

function f() {
  // eslint-disable-next-line no-debugger
  debugger;
}

self.onmessage = function (event) {
  switch (event.data) {
    case "ping":
      // eslint-disable-next-line no-debugger
      debugger;
      postMessage("pong");
      break;
  }
};
