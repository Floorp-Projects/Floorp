// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Debug.prototype.hooks

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
gc();  // don't assert marking dbg.hooks
var h = dbg.hooks;
assertEq(typeof h, 'object');
assertEq(Object.getOwnPropertyNames(h).length, 0);
assertEq(Object.getPrototypeOf(h), Object.prototype);

assertThrows(function () { dbg.hooks = null; }, TypeError);
assertThrows(function () { dbg.hooks = "bad"; }, TypeError);

assertEq(Object.getOwnPropertyNames(dbg).length, 0);
var desc = Object.getOwnPropertyDescriptor(Debug.prototype, "hooks");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);

assertThrows(function () { desc.get(); }, TypeError);
assertThrows(function () { desc.get.call(undefined); }, TypeError);
assertThrows(function () { desc.get.call(Debug.prototype); }, TypeError);
assertEq(desc.get.call(dbg), h);

assertThrows(function () { desc.set(); }, TypeError);
assertThrows(function () { desc.set.call(dbg); }, TypeError);
assertThrows(function () { desc.set.call({}, {}); }, TypeError);
assertThrows(function () { desc.set.call(Debug.prototype, {}); }, TypeError);

reportCompare(0, 0, 'ok');
