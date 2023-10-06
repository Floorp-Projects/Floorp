"use strict";

self.onmessage = function (event) {
  if (event.data !== "resolve") {
    return;
  }
  // This then-handler should be executed inside the top-level event loop,
  // within the context of the worker's global.
  Promise.resolve().then(function () {
    self.onmessage = function (e) {
      if (e.data !== "pause") {
        return;
      }
      // This then-handler should be executed inside the top-level event loop,
      // within the context of the worker's global. Because the debugger
      // statement here below enters a nested event loop, the then-handler
      // should not be executed until the debugger statement returns.
      Promise.resolve().then(function () {
        postMessage("resumed");
      });
      // eslint-disable-next-line no-debugger
      debugger;
    };
    postMessage("resolved");
  });
};
