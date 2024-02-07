var g = newGlobal({newCompartment: true});
g.eval("x = Object.create({}, {y: {value: 1}});");
var dbg = new Debugger();
var dobj = dbg.addDebuggee(g);
var v = dobj.getOwnPropertyDescriptor("x").value;
var ex;
try {
  v.defineProperties({y: {value: 2}});
} catch (e) {
  ex = e;
}
nukeAllCCWs();
assertEq(ex instanceof TypeError, true);
assertEq(ex.stack, ""); // Nuked stack CCW.
