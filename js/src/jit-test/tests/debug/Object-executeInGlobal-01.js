// Debugger.Object.prototype.executeInGlobal basics

var g = newGlobal();
var h = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var hw = dbg.addDebuggee(h);

g.y = "Bitte Orca";
h.y = "Visiter";
var y = "W H O K I L L";
assertEq(gw.executeInGlobal('y').return, "Bitte Orca");
assertEq(hw.executeInGlobal('y').return, "Visiter");
