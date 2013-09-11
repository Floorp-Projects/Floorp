// If frame.onStep returns null, the debuggee terminates.

var g = newGlobal();
g.eval("function h() { debugger; }");

var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    if (hits == 1) {
        var rv = frame.eval("h();\n" +
                            "throw 'fail';\n");
        assertEq(rv, null);
    } else {
        frame.older.onStep = function () { return null; };
    }
};
g.h();
assertEq(hits, 2);
