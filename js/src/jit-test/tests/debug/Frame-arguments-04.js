// frame.arguments works for all live frames

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    for (var i = 0; i <= 4; i++) {
        assertEq(frame.arguments.length, 1);
        assertEq(frame.arguments[0], i);
        frame = frame.older;
    }
    assertEq(frame, null);
    hits++;
};

g.eval("function f(n) { if (n == 0) debugger; else f(n - 1); }");
g.f(4);
assertEq(hits, 1);
