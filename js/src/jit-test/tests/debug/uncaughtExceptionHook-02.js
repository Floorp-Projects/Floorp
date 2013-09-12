// Returning a bad resumption value causes an exception that is reported to the
// uncaughtExceptionHook.

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function () { return {oops: "bad resumption value"}; };
dbg.uncaughtExceptionHook = function (exc) {
    assertEq(exc instanceof TypeError, true);
    return {return: "pass"};
};

assertEq(g.eval("debugger"), "pass");
