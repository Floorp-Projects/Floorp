"use strict";

self.addEventListener("message", ({ data }) => foo(data));

function foo(data) {
  throw new Error("worker2");
}
