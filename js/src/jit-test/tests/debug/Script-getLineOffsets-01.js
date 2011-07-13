// getLineOffsets on a line that is definitely outside a script returns an empty array.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var offs = frame.script.getLineOffsets(g.line0 + 2);
    assertEq(Array.isArray(offs), true);
    assertEq(offs.length, 0);
    hits++;
};
g.eval("var line0 = Error().lineNumber; debugger;");
assertEq(hits, 1);
