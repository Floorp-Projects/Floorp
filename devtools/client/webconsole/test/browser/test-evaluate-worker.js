"use strict";

self.addEventListener("message", ({ data }) => foo(data));

// This paramater is accessed by the debugger
// eslint-disable-next-line no-unused-vars
function foo(data) {
  // eslint-disable-next-line no-debugger
  debugger;
}
