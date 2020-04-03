// getVariable works in function scopes.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var lexicalEnv = frame.environment;
    var varEnv = lexicalEnv.parent;
    assertEq(varEnv.getVariable("a"), 1);
    assertEq(varEnv.getVariable("b"), 2);
    assertEq(varEnv.getVariable("c"), 3);
    assertEq(varEnv.getVariable("d"), 7);
    assertEq(lexicalEnv.getVariable("e"), 8);
    hits++;
};
g.eval("function f(a, [b, c]) { var d = a + b + c + 1; let e = d + 1; debugger; }");
g.f(1, [2, 3]);
assertEq(hits, 1);
