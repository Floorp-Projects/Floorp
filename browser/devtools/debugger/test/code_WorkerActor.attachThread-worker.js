"use strict";

function f() {
  var a = 1;
  var b = 2;
  var c = 3;
}

self.onmessage = function (event) {
  if (event.data == "ping") {
    f()
    postMessage("pong");
  }
};

postMessage("load");
