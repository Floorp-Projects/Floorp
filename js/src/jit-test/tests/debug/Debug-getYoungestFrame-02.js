// |jit-test| debug
// Hooks and Debug.prototype.getYoungestFrame produce the same Frame object.

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var hits = 0;
var savedFrame, savedCallee;
dbg.hooks = {
    debuggerHandler: function (frame) {
        assertEq(frame, savedFrame);
        assertEq(frame.live, true);
        assertEq(frame.callee, savedCallee);
        hits++;
    }
};
g.h = function () {
    savedFrame = dbg.getYoungestFrame();
    savedCallee = savedFrame.callee;
    assertEq(savedCallee.name, "f");
};
g.eval("function f() { h(); debugger; }");
g.f();
assertEq(hits, 1);
