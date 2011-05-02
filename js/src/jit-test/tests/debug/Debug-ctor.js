// |jit-test| debug

load(libdir + 'asserts.js');

// Debug rejects arguments that aren't cross-compartment wrappers.
assertThrowsInstanceOf(function () { Debug(); }, TypeError);
assertThrowsInstanceOf(function () { Debug(null); }, TypeError);
assertThrowsInstanceOf(function () { Debug(true); }, TypeError);
assertThrowsInstanceOf(function () { Debug(42); }, TypeError);
assertThrowsInstanceOf(function () { Debug("bad"); }, TypeError);
assertThrowsInstanceOf(function () { Debug(function () {}); }, TypeError);
assertThrowsInstanceOf(function () { Debug(this); }, TypeError);
assertThrowsInstanceOf(function () { new Debug(); }, TypeError);
assertThrowsInstanceOf(function () { new Debug(null); }, TypeError);
assertThrowsInstanceOf(function () { new Debug(true); }, TypeError);
assertThrowsInstanceOf(function () { new Debug(42); }, TypeError);
assertThrowsInstanceOf(function () { new Debug("bad"); }, TypeError);
assertThrowsInstanceOf(function () { new Debug(function () {}); }, TypeError);
assertThrowsInstanceOf(function () { new Debug(this); }, TypeError);

// From the main compartment, creating a Debug on a sandbox compartment.
var g = newGlobal('new-compartment');
var dbg = new Debug(g);
assertEq(dbg instanceof Debug, true);
assertEq(Object.getPrototypeOf(dbg), Debug.prototype);

// The reverse.
var g2 = newGlobal('new-compartment');
g2.debuggeeGlobal = this;
g2.eval("var dbg = new Debug(debuggeeGlobal);");
assertEq(g2.eval("dbg instanceof Debug"), true);

// The Debug constructor from this compartment will not accept as its argument
// an Object from this compartment. Shenanigans won't fool the membrane.
g2.outer = this;
assertThrowsInstanceOf(function () { g2.eval("outer.Debug(outer.Object())"); }, TypeError);
