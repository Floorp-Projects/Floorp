// obj.environment is undefined when the referent is not a JS function.

var g = newGlobal('new-compartment')
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
assertEq(gw.environment, undefined);

g.eval("var r = /x/;");
var rw = gw.getOwnPropertyDescriptor("r").value;
assertEq(rw.class, "RegExp");
assertEq(rw.environment, undefined);

// Native function.
var fw = gw.getOwnPropertyDescriptor("parseInt").value;
assertEq(fw.class, "Function");
assertEq(fw.environment, undefined);

