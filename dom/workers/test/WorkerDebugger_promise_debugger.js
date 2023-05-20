"use strict";

var self = this;

self.onmessage = function (event) {
  if (event.data !== "resolve") {
    return;
  }
  // This then-handler should be executed inside the top-level event loop,
  // within the context of the debugger's global.
  Promise.resolve().then(function () {
    var dbg = new Debugger(global);
    dbg.onDebuggerStatement = function () {
      self.onmessage = function (e) {
        if (e.data !== "resume") {
          return;
        }
        // This then-handler should be executed inside the nested event loop,
        // within the context of the debugger's global.
        Promise.resolve().then(function () {
          postMessage("resumed");
          leaveEventLoop();
        });
      };
      // Test bug 1392540 where DOM Promises from debugger principal
      // where frozen while hitting a worker breakpoint.
      Promise.resolve().then(() => {
        postMessage("paused");
      });
      enterEventLoop();
    };
    postMessage("resolved");
  });
};
