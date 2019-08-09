"use strict";

self.addEventListener("message", ({ data }) => foo(data));

var w = new Worker("test-error-worker2.js");
w.postMessage({});

function foo(data) {
  switch (data) {
    case 1:
      throw new Error("hello");
    case 2:
      /* eslint-disable */
      throw "there";
    case 3:
      throw new DOMException("dom");
  }
}
