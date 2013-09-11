// frame.offset throws if !frame.live.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = Debugger(g);
var f;
dbg.onDebuggerStatement = function (frame) { f = frame; };
g.eval("debugger;");
assertEq(f.live, false);
assertThrowsInstanceOf(function () { f.offset; }, Error);
