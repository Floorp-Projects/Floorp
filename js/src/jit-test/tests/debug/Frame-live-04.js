// frame.live is false for frames discarded during uncatchable error unwinding.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits = 0;
var snapshot;
dbg.onDebuggerStatement = function (frame) {
    var stack = [];
    for (var f = frame; f; f = f.older) {
        if (f.type === "call" && f.script !== null)
            stack.push(f);
    }
    snapshot = stack;
    if (hits++ === 0)
        assertEq(frame.eval("x();"), null);
    else
        return null;
};

g.eval("function z() { debugger; }");
g.eval("function y() { z(); }");
g.eval("function x() { y(); }");
assertEq(g.eval("debugger; 'ok';"), "ok");
assertEq(hits, 2);
assertEq(snapshot.length, 3);
for (var i = 0; i < snapshot.length; i++)
    assertEq(snapshot[i].live, false);
