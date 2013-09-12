// Hooks and Debugger.prototype.getNewestFrame produce the same Frame object.

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
var savedFrame, savedCallee;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame, savedFrame);
    assertEq(frame.live, true);
    assertEq(frame.callee, savedCallee);
    hits++;
};
g.h = function () {
    savedFrame = dbg.getNewestFrame();
    savedCallee = savedFrame.callee;
    assertEq(savedCallee.name, "f");
};
g.eval("function f() { h(); debugger; }");
g.f();
assertEq(hits, 1);
