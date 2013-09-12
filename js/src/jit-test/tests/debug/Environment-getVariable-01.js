// Environment.prototype.getVariable does not see variables bound in enclosing scopes.

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.environment.getVariable("x"), 13);
    assertEq(frame.environment.getVariable("k"), undefined);
    assertEq(frame.environment.find("k").getVariable("k"), 3);
    hits++;
};
g.eval("var k = 3; function f(x) { debugger; }");
g.f(13);
assertEq(hits, 1);
