load(libdir + 'asserts.js');

// Debug rejects arguments that aren't cross-compartment wrappers.
assertThrowsInstanceOf(function () { Debug(null); }, TypeError);
assertThrowsInstanceOf(function () { Debug(true); }, TypeError);
assertThrowsInstanceOf(function () { Debug(42); }, TypeError);
assertThrowsInstanceOf(function () { Debug("bad"); }, TypeError);
assertThrowsInstanceOf(function () { Debug(function () {}); }, TypeError);
assertThrowsInstanceOf(function () { Debug(this); }, TypeError);
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
