// Don't crash when getting the Debugger.Environment of a frame inside
// Function.prototype.

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onEnterFrame = function (frame) { frame.environment; };
g.Function.prototype();
