// The body of a for-of loop does not run if the target is an empty typed array.

for (x of new Int16Array(0))
    throw "FAIL";
for (x of new Float32Array(0))
    throw "FAIL";

var a = new Int8Array([0, 1, 2, 3]).subarray(2, 2);
assertEq(a.length, 0);
for (v of a)
    throw "FAIL";
