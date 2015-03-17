"use strict";

var dbg = new Debugger(global);
dbg.onDebuggerStatement = function (frame) {
  frame.eval("postMessage('debugger');");
};
