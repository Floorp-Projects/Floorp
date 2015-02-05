// Debugger.Object.prototype.makeDebuggeeValue creates only one
// Debugger.Object instance for each debuggee object.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

g.eval("var x = { 'now playing': 'Joy Division' };");
g.eval("var y = { 'mood': 'bleak' };");

wx = gw.makeDebuggeeValue(g.x);
assertEq(wx, gw.makeDebuggeeValue(g.x));
assertEq(wx === g.x, false);
assertEq("now playing" in wx, false);
assertEq(wx.getOwnPropertyNames().indexOf("now playing"), 0);
wx.commentary = "deconstruction";
assertEq("deconstruction" in g.x, false);

wy = gw.makeDebuggeeValue(g.y);
assertEq(wy === wx, false);
wy.commentary = "reconstruction";
assertEq(wx.commentary, "deconstruction");

// Separate debuggers get separate Debugger.Object instances, but both
// instances' referents are the same underlying object.
var dbg2 = new Debugger();
var gw2 = dbg2.addDebuggee(g);
w2x = gw2.makeDebuggeeValue(g.x);
assertEq(wx === w2x, false);
w2x.defineProperty("breadcrumb", { value: "pumpernickel" });
assertEq(wx.getOwnPropertyDescriptor("breadcrumb").value, "pumpernickel");

// Non-objects are already debuggee values.
assertEq(gw.makeDebuggeeValue("foonting turlingdromes"), "foonting turlingdromes");
assertEq(gw.makeDebuggeeValue(true), true);
assertEq(gw.makeDebuggeeValue(false), false);
assertEq(gw.makeDebuggeeValue(null), null);
assertEq(gw.makeDebuggeeValue(1729), 1729);
assertEq(gw.makeDebuggeeValue(Math.PI), Math.PI);
assertEq(gw.makeDebuggeeValue(undefined), undefined);
var s = g.eval("Symbol('Stavromula Beta')");
assertEq(gw.makeDebuggeeValue(s), s);
