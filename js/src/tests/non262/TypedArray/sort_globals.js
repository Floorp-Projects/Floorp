// TypedArray.prototype.sort should work across globals
let g2 = newGlobal();
assertEqArray(
    Int32Array.prototype.sort.call(new g2.Int32Array([3, 2, 1])),
    new Int32Array([1, 2, 3])
);

if (typeof reportCompare === "function")
    reportCompare(true, true);
