// Property values that are objects are reflected as Debugger.Objects.

var g = newGlobal({newCompartment: true});
var dbg = Debugger();
var gobj = dbg.addDebuggee(g);
g.self = g;
var desc = gobj.getOwnPropertyDescriptor("self");
assertEq(desc.value, gobj.makeDebuggeeValue(g));
