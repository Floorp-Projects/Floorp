// getNewestFrame basics.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
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
assertEq(frame.onStack, false);
assertThrowsInstanceOf(function () { frame.older; }, Error);
