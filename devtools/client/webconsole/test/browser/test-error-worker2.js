"use strict";

self.addEventListener("message", ({ data }) => foo(data));

function foo() {
  throw new Error("worker2");
}
