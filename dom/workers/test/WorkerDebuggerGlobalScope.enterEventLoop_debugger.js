"use strict";

var frames = [];

var dbg = new Debugger(global);
dbg.onDebuggerStatement = function (frame) {
  frames.push(frame);
  postMessage("paused");
  enterEventLoop();
  frames.pop();
  postMessage("resumed");
};

this.onmessage = function (event) {
  switch (event.data) {
  case "eval":
    frames[frames.length - 1].eval("f()");
    postMessage("evalled");
    break;

  case "ping":
    postMessage("pong");
    break;

  case "resume":
    leaveEventLoop();
    break;
  };
};
