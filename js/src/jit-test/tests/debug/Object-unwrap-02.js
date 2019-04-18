// Debugger.Object.prototype.unwrap unwraps Debugger.Objects referring to
// cross-compartment wrappers.

var dbg = new Debugger();

var g1 = newGlobal({newCompartment: true});
var dg1 = dbg.addDebuggee(g1);
assertEq(dg1.unwrap(), dg1);

var g2 = newGlobal({newCompartment: true});
var dg2 = dbg.addDebuggee(g2);

var dg1g2 = dg1.makeDebuggeeValue(g2);
assertEq(dg1g2.unwrap(), dg2.makeDebuggeeValue(g2));

// Try an ordinary object, not a global.
var g2o = g2.Object();
var dg2o = dg2.makeDebuggeeValue(g2o);
var dg1g2o = dg1.makeDebuggeeValue(g2o);
assertEq(dg1g2o.unwrap(), dg2o);
assertEq(dg1g2o.unwrap().unwrap(), dg2o);
