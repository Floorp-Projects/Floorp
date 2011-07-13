// frame.eval returns null if the eval code fails with an uncatchable error.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    if (hits++ === 0)
        assertEq(frame.eval("debugger;"), null);
    else
        return null;
};
assertEq(g.eval("debugger; 'ok';"), "ok");
assertEq(hits, 2);
