// obj.getOwnPropertyDescriptor works when obj is a transparent cross-compartment wrapper.

var g1 = newGlobal('new-compartment');
var g2 = newGlobal('new-compartment');
g1.next = g2;

// This test is a little hard to follow, especially the !== assertions.
//
// Bottom line: the value of a property of g1 can only be an object in g1's
// compartment, so any Debugger.Objects obtained by calling
// g1obj.getOwnPropertyDescriptor should all have referents in g1's
// compartment.

var dbg = new Debugger;
var g1obj = dbg.addDebuggee(g1);
var g2obj = dbg.addDebuggee(g2);
var wobj = g1obj.getOwnPropertyDescriptor("next").value;
assertEq(wobj instanceof Debugger.Object, true);
assertEq(wobj !== g2obj, true);  // referents are in two different compartments

g2.x = "ok";
assertEq(wobj.getOwnPropertyDescriptor("x").value, "ok");

g1.g2min = g2.min = g2.Math.min;
g2.eval("Object.defineProperty(this, 'y', {get: min});");
assertEq(g2.y, Infinity);
var wmin = wobj.getOwnPropertyDescriptor("y").get;
assertEq(wmin !== g2obj.getOwnPropertyDescriptor("min").value, true);  // as above
assertEq(wmin, g1obj.getOwnPropertyDescriptor("g2min").value);
