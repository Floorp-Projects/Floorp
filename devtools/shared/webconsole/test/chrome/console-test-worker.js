"use strict";

console.log("Log from worker init");

function f() {
  const a = 1;
  const b = 2;
  const c = 3;
  return { a, b, c };
}

self.onmessage = function (event) {
  if (event.data == "ping") {
    f();
    postMessage("pong");
  } else if (event.data?.type == "log") {
    console.log(event.data.message);
  }
};

postMessage("load");
