load(libdir + "asserts.js");

assertEq(BigInt.asUintN(32, -1n), 0xffffffffn);
assertThrowsInstanceOf(() => BigInt.asUintN(2**32 - 1, -1n), RangeError);
assertThrowsInstanceOf(() => BigInt.asUintN(2**32, -1n), RangeError);
assertThrowsInstanceOf(() => BigInt.asUintN(2**53 - 1, -1n), RangeError);
