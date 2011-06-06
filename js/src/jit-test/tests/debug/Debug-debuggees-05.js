// |jit-test| debug
// addDebuggee returns different Debug.Object wrappers for different Debug objects.

var g = newGlobal('new-compartment');
var dbg1 = new Debug;
var gw1 = dbg1.addDebuggee(g);
var dbg2 = new Debug;
var gw2 = dbg2.addDebuggee(g);
assertEq(gw1 !== gw2, true);
