// Debugger.Object.prototype.executeInGlobal throws when asked to evaluate in a CCW of a global.

load(libdir + 'asserts.js');

var dbg = new Debugger();

var g1 = newGlobal({newCompartment: true});
var dg1 = dbg.addDebuggee(g1);

var g2 = newGlobal({newCompartment: true});
var dg2 = dbg.addDebuggee(g2);

// Generate a Debugger.Object viewing g2 from g1's compartment.
var dg1wg2 = dg1.makeDebuggeeValue(g2);
assertEq(dg1wg2.unwrap(), dg2.makeDebuggeeValue(g2));
assertThrowsInstanceOf(function () { dg1wg2.executeInGlobal('1'); }, TypeError);
assertThrowsInstanceOf(function () { dg1wg2.executeInGlobalWithBindings('x', { x: 1 }); }, TypeError);

// These, however, should not throw.
assertEq(dg1.executeInGlobal('1729').return, 1729);
assertEq(dg1.executeInGlobalWithBindings('x', { x: 1729 }).return, 1729);
