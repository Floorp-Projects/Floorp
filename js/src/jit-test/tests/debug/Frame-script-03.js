// frame.script can create a Debugger.Script for a JS_Evaluate* script.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var s;
dbg.onDebuggerStatement = function (frame) { s = frame.script; };
g.evaluate("debugger;");
assertEq(s instanceof Debugger.Script, true);
