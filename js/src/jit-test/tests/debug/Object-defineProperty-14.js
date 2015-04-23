// defineProperty accepts undefined for desc.get/set.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

gw.defineProperty("p", {get: undefined, set: undefined});

var desc = g.eval("Object.getOwnPropertyDescriptor(this, 'p')");
assertEq("get" in desc, true);
assertEq("set" in desc, true);
assertEq(desc.get, undefined);
assertEq(desc.set, undefined);
