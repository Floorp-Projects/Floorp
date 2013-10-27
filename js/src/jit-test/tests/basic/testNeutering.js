var ab = new ArrayBuffer(4);
var i32 = new Int32Array(ab);
i32[0] = 42;
neuter(ab);
assertEq(i32.length, 0);
assertEq(ab.byteLength, 0);
assertEq(i32[0], undefined);

var ab = new ArrayBuffer(12);
var i32 = new Int32Array(ab);
i32[0] = 42;
neuter(ab);
assertEq(i32.length, 0);
assertEq(ab.byteLength, 0);
assertEq(i32[0], undefined);

var ab = new ArrayBuffer(4096);
var i32 = new Int32Array(ab);
i32[0] = 42;
neuter(ab);
assertEq(i32.length, 0);
assertEq(ab.byteLength, 0);
assertEq(i32[0], undefined);
