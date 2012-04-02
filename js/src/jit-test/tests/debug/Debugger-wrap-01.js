// Debugger.prototype.wrap creates only one Debugger.Object instance for each debuggee object.
var g = newGlobal('new-compartment');
var dbg = new Debugger(g);

g.eval("var x = { 'now playing': 'Joy Division' };");
g.eval("var y = { 'mood': 'bleak' };");

wx = dbg.wrap(g.x);
assertEq(wx, dbg.wrap(g.x));
assertEq(wx === g.x, false);
assertEq("now playing" in wx, false);
assertEq(wx.getOwnPropertyNames().indexOf("now playing"), 0);
wx.commentary = "deconstruction";
assertEq("deconstruction" in g.x, false);

wy = dbg.wrap(g.y);
assertEq(wy === wx, false);
wy.commentary = "reconstruction";
assertEq(wx.commentary, "deconstruction");

// Separate debuggers get separate wrappers, but they both view the same underlying object.
var dbg2 = new Debugger(g);
w2x = dbg2.wrap(g.x);
assertEq(wx === w2x, false);
w2x.defineProperty("breadcrumb", { value: "pumpernickel" });
assertEq(wx.getOwnPropertyDescriptor("breadcrumb").value, "pumpernickel");

// Trying to wrap things that aren't objects should pass them through unchanged.
assertEq(dbg.wrap("foonting turlingdromes"), "foonting turlingdromes");
assertEq(dbg.wrap(true), true);
assertEq(dbg.wrap(false), false);
assertEq(dbg.wrap(null), null);
assertEq(dbg.wrap(1729), 1729);
assertEq(dbg.wrap(undefined), undefined);
