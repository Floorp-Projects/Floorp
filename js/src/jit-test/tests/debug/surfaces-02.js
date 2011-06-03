// Debug.prototype.hooks

load(libdir + 'asserts.js');

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
gc();  // don't assert marking dbg.hooks
var h = dbg.hooks;
assertEq(typeof h, 'object');
assertEq(Object.getOwnPropertyNames(h).length, 0);
assertEq(Object.getPrototypeOf(h), Object.prototype);

assertThrowsInstanceOf(function () { dbg.hooks = null; }, TypeError);
assertThrowsInstanceOf(function () { dbg.hooks = "bad"; }, TypeError);

assertEq(Object.getOwnPropertyNames(dbg).length, 0);
var desc = Object.getOwnPropertyDescriptor(Debug.prototype, "hooks");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);

assertThrowsInstanceOf(function () { desc.get(); }, TypeError);
assertThrowsInstanceOf(function () { desc.get.call(undefined); }, TypeError);
assertThrowsInstanceOf(function () { desc.get.call(Debug.prototype); }, TypeError);
assertEq(desc.get.call(dbg), h);

assertThrowsInstanceOf(function () { desc.set(); }, TypeError);
assertThrowsInstanceOf(function () { desc.set.call(dbg); }, TypeError);
assertThrowsInstanceOf(function () { desc.set.call({}, {}); }, TypeError);
assertThrowsInstanceOf(function () { desc.set.call(Debug.prototype, {}); }, TypeError);
