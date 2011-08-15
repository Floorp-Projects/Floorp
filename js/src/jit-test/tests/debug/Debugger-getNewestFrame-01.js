// getNewestFrame basics.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
assertEq(dbg.getNewestFrame(), null);

var global = this;
var frame;
function f() {
    frame = dbg.getNewestFrame();
    assertEq(frame instanceof Debugger.Frame, true);
    assertEq(frame.type, "eval");
    assertEq(frame.older, null);
}
g.h = this;
g.eval("h.f()");
assertEq(frame.live, false);
assertThrowsInstanceOf(function () { frame.older; }, Error);
