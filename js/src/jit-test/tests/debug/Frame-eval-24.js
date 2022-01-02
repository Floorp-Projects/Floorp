// Make sure the getVariable/setVariable/eval functions work correctly with
// unaliased locals.
var g = newGlobal({newCompartment: true});
g.eval(`
function g() { debugger; };
function f(arg) {
    var y = arg - 3;
    var a1 = 1;
    var a2 = 1;
    var b = arg + 9;
    var z = function() { return a1 + a2; };
    g();
    return y * b; // To prevent the JIT from optimizing out y and b.
};`);

var dbg = new Debugger(g);

dbg.onDebuggerStatement = function handleDebugger(frame) {
    assertEq(frame.older.eval("y + b").return, 26);
    assertEq(frame.older.environment.getVariable("y"), 7);
    frame.older.environment.setVariable("b", 4);
    assertEq(frame.older.eval("y + b").return, 11);
};

g.f(10);
