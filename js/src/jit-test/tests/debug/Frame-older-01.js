// |jit-test| debug
// Basic call chain.

var g = newGlobal();
var result = null;
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    var a = [];
    assertEq(frame === frame.older, false);
    for (; frame; frame = frame.older)
        a.push(frame.type === 'call' ? frame.callee.name : frame.type);
    a.reverse();
    result = a.join(", ");
};

g.eval("function first() { return second(); }");
g.eval("function second() { return eval('third()'); }");
g.eval("function third() { debugger; }");
g.evaluate("first();");
assertEq(result, "global, first, second, eval, third");
