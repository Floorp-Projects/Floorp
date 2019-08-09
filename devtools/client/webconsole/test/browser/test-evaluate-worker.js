"use strict";

self.addEventListener("message", ({ data }) => foo(data));

function foo(data) {
  // eslint-disable-next-line no-debugger
  debugger;
}
