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
      self.onmessage = function (event) {
        if (event.data !== "resume") {
          return;
        }
        // This then-handler should be executed inside the nested event loop,
        // within the context of the debugger's global.
        Promise.resolve().then(function () {
          postMessage("resumed");
          leaveEventLoop();
        });
      };
      postMessage("paused");
      enterEventLoop();
    };
    postMessage("resolved");
  });
};
