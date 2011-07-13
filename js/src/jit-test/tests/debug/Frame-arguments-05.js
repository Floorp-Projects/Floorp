// frame.arguments is "live" (it reflects assignments to arguments).

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var log = '';
var args;
dbg.onDebuggerStatement = function (frame) {
    if (args === undefined)
        args = frame.arguments;
    else
        assertEq(frame.arguments, args);
    log += args[0];
    assertEq(frame.eval("x = '0';").return, '0');
    log += args[0];
};

g.eval("function f(x) { x = '2'; debugger; x = '3'; debugger; }");
g.f("1");
assertEq(log, "2030");
