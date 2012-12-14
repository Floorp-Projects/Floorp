// Bug 744731 - findScripts() finds active debugger frame.eval scripts.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    dbg.onDebuggerStatement = function (frame) {
        hits++;
        assertEq(dbg.findScripts().indexOf(dbg.getNewestFrame().script) !== -1, true);
    };
    frame.eval("debugger;");
};
g.eval("debugger;");
assertEq(hits, 1);
