// |reftest| skip-if(!xulRuntime.shell)

// Ensure that TypedArrays throw when attempting to sort a detached ArrayBuffer
assertThrowsInstanceOf(() => {
    let buffer = new ArrayBuffer(32);
    let array  = new Int32Array(buffer);
    neuter(buffer, "change-data");
    array.sort();
}, TypeError);

// Ensure that TypedArray.prototype.sort will not sort non-TypedArrays
assertThrowsInstanceOf(() => {
    let array = [4, 3, 2, 1];
    Int32Array.prototype.sort.call(array);
}, TypeError);

assertThrowsInstanceOf(() => {
    Int32Array.prototype.sort.call({a: 1, b: 2});
}, TypeError);

assertThrowsInstanceOf(() => {
    Int32Array.prototype.sort.call(Int32Array.prototype);
}, TypeError);

assertThrowsInstanceOf(() => {
    let buf = new ArrayBuffer(32);
    Int32Array.prototype.sort.call(buf);
}, TypeError);

// Ensure that comparator errors are propagataed
function badComparator(x, y) {
    if (x == 99 && y == 99)
        throw new TypeError;
    return x - y;
}

assertThrowsInstanceOf(() => {
    let array = new Uint8Array([99, 99, 99, 99]);
    array.sort(badComparator);
}, TypeError);

assertThrowsInstanceOf(() => {
    let array = new Uint8Array([1, 99, 2, 99]);
    array.sort(badComparator);
}, TypeError);


if (typeof reportCompare === "function")
    reportCompare(true, true);
