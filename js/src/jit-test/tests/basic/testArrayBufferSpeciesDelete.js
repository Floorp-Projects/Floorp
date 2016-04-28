delete ArrayBuffer[Symbol.species];
var a = new Uint8Array(new Uint8Array([1, 2]));
assertEq(a.length, 2);
assertEq(a[0], 1);
assertEq(a[1], 2);
