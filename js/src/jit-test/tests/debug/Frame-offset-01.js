// frame.offset throws if !frame.onStack.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var f;
dbg.onDebuggerStatement = function (frame) { f = frame; };
g.eval("debugger;");
assertEq(f.onStack, false);
assertThrowsInstanceOf(function () { f.offset; }, Error);
