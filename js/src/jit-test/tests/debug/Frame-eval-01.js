// simplest possible test of Debugger.Frame.prototype.eval

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var c;
dbg.onDebuggerStatement = function (frame) { c = frame.eval("2 + 2"); };
g.eval("debugger;");
assertEq(c.return, 4);
