// If !frame.onStack, frame.environment throws.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var frame;
g.h = function () { frame = dbg.getNewestFrame(); };
g.eval("h();");
assertEq(frame.onStack, false);
assertThrowsInstanceOf(function () { frame.environment; }, Error);
