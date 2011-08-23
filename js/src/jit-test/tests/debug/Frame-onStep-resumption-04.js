// If frame.onStep returns null, debuggee catch and finally blocks are skipped.

var g = newGlobal('new-compartment');
g.eval("function h() { debugger; }");

var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    if (hits == 1) {
        var rv = frame.eval("try {\n" +
                            "    h();\n" +
                            "    throw 'fail';\n" +
                            "} catch (exc) {\n" +
                            "    caught = exc;\n" +
                            "} finally {\n" +
                            "    finallyHit = true;\n" +
                            "}\n");
        assertEq(rv, null);
    } else {
        frame.older.onStep = function () {
            this.onStep = undefined;
            return null;
        };
    }
};

g.h();
assertEq(hits, 2);
assertEq("caught" in g, false);
assertEq("finallyHit" in g, false);
