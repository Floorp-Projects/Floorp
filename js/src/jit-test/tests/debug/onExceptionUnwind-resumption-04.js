// Check that an onExceptionUnwind hook can force a frame to terminate.

var g = newGlobal();
var dbg = Debugger(g);
g.eval("function f() { throw 'ksnife'; }");
var log = '';
dbg.onDebuggerStatement = function (frame) {
    log += 'd1';
    assertEq(frame.eval("f();"), null);
    log += 'd2';
};
dbg.onExceptionUnwind = function (frame, exc) {
    log += 'u';
    return null;
};
g.eval("debugger;");
assertEq(log, "d1ud2");
