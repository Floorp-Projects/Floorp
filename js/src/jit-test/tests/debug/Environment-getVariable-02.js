// getVariable works in function scopes.

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.environment.getVariable("a"), 1);
    assertEq(frame.environment.getVariable("b"), 2);
    assertEq(frame.environment.getVariable("c"), 3);
    assertEq(frame.environment.getVariable("d"), 4);
    assertEq(frame.environment.getVariable("e"), 5);
    hits++;
};
g.eval("function f(a, [b, c]) { var d = c + 1; let e = d + 1; debugger; }");
g.f(1, [2, 3]);
assertEq(hits, 1);
