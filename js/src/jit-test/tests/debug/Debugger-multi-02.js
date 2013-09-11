// Test adding hooks during dispatch. The behavior is deterministic and "nice",
// but mainly what we are checking here is that we do not crash due to
// modifying a data structure while we're iterating over it.

var g = newGlobal();
var n = 0;
var hits;

function addDebugger() {
    var dbg = new Debugger(g);
    dbg.onDebuggerStatement = function (stack) {
        hits++;
        addDebugger();
    };
}

addDebugger();  // now there is one enabled Debugger
hits = 0;
g.eval("debugger;");  // after this there are two
assertEq(hits, 1);

hits = 0;
g.eval("debugger;");  // after this there are four
assertEq(hits, 2);

hits = 0;
g.eval("debugger;");  // after this there are eight
assertEq(hits, 4);

hits = 0;
g.eval("debugger;");
assertEq(hits, 8);
