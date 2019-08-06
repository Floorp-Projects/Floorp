// |jit-test| --no-bigint

load(libdir + "asserts.js");

assertEq(DataView.prototype.hasOwnProperty('getInt32'), true)
assertEq(DataView.prototype.hasOwnProperty('getBigInt64'), false);
assertEq(DataView.prototype.hasOwnProperty('getBigUint64'), false);
assertEq(DataView.prototype.hasOwnProperty('setBigInt64'), false);
assertEq(DataView.prototype.hasOwnProperty('setBigUint64'), false);

assertThrowsInstanceOf(() => BigInt, ReferenceError);
