// getVariable sees variables in function scopes added by non-strict direct eval.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var v;
dbg.onDebuggerStatement = function (frame) {
    v = frame.environment.getVariable("x");
};

g.eval("function f(s) { eval(s); debugger; }");
g.f("var x = 'Q';");
assertEq(v, 'Q');
