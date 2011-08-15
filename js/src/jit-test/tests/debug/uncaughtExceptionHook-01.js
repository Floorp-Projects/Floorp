// Uncaught exceptions in the debugger itself are delivered to the
// uncaughtExceptionHook.

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var log;
dbg.onDebuggerStatement = function () {
    log += 'x';
    throw new TypeError("fail");
};
dbg.uncaughtExceptionHook = function (exc) {
    assertEq(this, dbg);
    assertEq(exc instanceof TypeError, true);
    log += '!';
};

log = '';
g.eval("debugger");
assertEq(log, 'x!');
