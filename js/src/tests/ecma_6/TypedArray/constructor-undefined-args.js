// Bug 1040402

var ab = new ArrayBuffer(16);

assertEq(new Int32Array(ab).length, 4);
assertEq(new Int32Array(ab, undefined).length, 4);
assertEq(new Int32Array(ab, undefined, undefined).length, 4);
assertEq(new Int32Array(ab, 0).length, 4);
assertEq(new Int32Array(ab, 0, undefined).length, 4);
assertEq(new Int32Array(ab, 4).length, 3);
assertEq(new Int32Array(ab, 4, undefined).length, 3);

if (typeof reportCompare === "function")
    reportCompare(true, true);
