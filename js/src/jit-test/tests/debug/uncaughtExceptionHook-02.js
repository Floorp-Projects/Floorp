// |jit-test| debug
// Returning a bad resumption value causes an exception that is reported to the
// uncaughtExceptionHook.

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
dbg.hooks = {debuggerHandler: function () { return {oops: "bad resumption value"}; }};
dbg.uncaughtExceptionHook = function (exc) {
    assertEq(exc instanceof TypeError, true);
    return {return: "pass"};
};

assertEq(g.eval("debugger"), "pass");
