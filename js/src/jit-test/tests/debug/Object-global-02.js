// Debugger.Object.prototype.global retrieves the correct global.

var dbg = new Debugger;
var g1 = newGlobal();
var g1w = dbg.addDebuggee(g1);
var g2 = newGlobal();
var g2w = dbg.addDebuggee(g2);

assertEq(g1w === g2w, false);
assertEq(g1w.global, g1w);
assertEq(g2w.global, g2w);

var g1ow = g1w.makeDebuggeeValue(g1.Object());
var g2ow = g2w.makeDebuggeeValue(g2.Object());
assertEq(g1ow.global, g1w);
assertEq(g2ow.global, g2w);

// mild paranoia
assertEq(g1ow.global === g1ow, false);
assertEq(g2ow.global === g2ow, false);

// The .global accessor doesn't unwrap.
assertEq(g1w.makeDebuggeeValue(g2.Object()).global, g1w);
assertEq(g2w.makeDebuggeeValue(g1.Object()).global, g2w);

