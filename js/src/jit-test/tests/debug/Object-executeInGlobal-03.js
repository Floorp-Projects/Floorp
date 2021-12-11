// Debugger.Object.prototype.executeInGlobal: closures capturing the global

var g = newGlobal({newCompartment: true});
var h = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var hw = dbg.addDebuggee(h);

g.x = "W H O K I L L";
h.x = "No Color";
var c1 = gw.executeInGlobal('(function () { return x; })').return;
var c2 = hw.executeInGlobal('(function () { return x; })').return;
var c3 = gw.executeInGlobalWithBindings('(function () { return x + y; })', { y:" In Rainbows" }).return;
var c4 = hw.executeInGlobalWithBindings('(function () { return x + y; })', { y:" In Rainbows" }).return;

assertEq(c1.call(null).return, "W H O K I L L");
assertEq(c2.call(null).return, "No Color");
assertEq(c3.call(null).return, "W H O K I L L In Rainbows");
assertEq(c4.call(null).return, "No Color In Rainbows");
