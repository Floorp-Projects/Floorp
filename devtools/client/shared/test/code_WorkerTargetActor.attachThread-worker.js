"use strict";

function f() {
  const a = 1;
  const b = 2;
  const c = 3;

  return [a, b, c];
}

self.onmessage = function (event) {
  if (event.data == "ping") {
    f();
    postMessage("pong");
  }
};

postMessage("load");
