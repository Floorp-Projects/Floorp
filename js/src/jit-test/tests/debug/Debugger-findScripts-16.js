// Bug 744731 - findScripts() finds active debugger evalInGlobal scripts.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    assertEq(dbg.findScripts().indexOf(dbg.getNewestFrame().script) !== -1, true);
};
gw.evalInGlobal("debugger;");
assertEq(hits, 1);
