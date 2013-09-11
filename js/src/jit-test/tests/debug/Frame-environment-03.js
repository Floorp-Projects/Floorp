// If !frame.live, frame.environment throws.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = Debugger(g);
var frame;
g.h = function () { frame = dbg.getNewestFrame(); };
g.eval("h();");
assertEq(frame.live, false);
assertThrowsInstanceOf(function () { frame.environment; }, Error);
