// Clearing a frame's onPop handler works.
var g = newGlobal('new-compartment');
g.eval("function f() { debugger; }");
var dbg = new Debugger(g);

var log;
dbg.onEnterFrame = function handleEnter(f) {
    log += "(";
    f.onPop = function handlePop() {
        assertEq("handlePop was called", "handlePop should never be called");
    };
};
dbg.onDebuggerStatement = function handleDebugger(f) {
    log += "d";
    assertEq(typeof f.onPop, "function");
    f.onPop = undefined;
};
log = '';
g.f();
assertEq(log, "(d");
