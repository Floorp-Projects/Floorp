// setVariable cannot modify the binding for a FunctionExpression's name.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var env = frame.environment.find("f");
    assertEq(env.getVariable("f"), frame.callee);
    assertThrowsInstanceOf(function () { env.setVariable("f", 0) }, TypeError);
    assertThrowsInstanceOf(function () { env.setVariable("f", frame.callee) }, TypeError);
    hits++;
};
g.eval("(function f() { debugger; })();");
assertEq(hits, 1);
