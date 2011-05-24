// |jit-test| debug
// frame.arguments is "live" (it reflects assignments to arguments).

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var log = '';
dbg.hooks = {
    debuggerHandler: function (frame) {
        log += frame.arguments[0];
    }
};

g.eval("function f(x) { x = '2'; debugger; x = '3'; debugger; }");
g.f("1");
assertEq(log, "23");
