// Ensure that TypedArrays throw when attempting to sort a detached ArrayBuffer
if (typeof detachArrayBuffer === "function") {
    assertThrowsInstanceOf(() => {
        let buffer = new ArrayBuffer(32);
        let array  = new Int32Array(buffer);
        detachArrayBuffer(buffer);
        array.sort();
    }, TypeError);
}

// Ensure detachment check works when buffer is detached in comparator.
if (typeof detachArrayBuffer === "function") {
    let detached = false;
    let ta = new Int32Array(3);
    assertThrowsInstanceOf(() => {
        ta.sort(function(a, b) {
            assertEq(detached, false);
            detached = true;
            detachArrayBuffer(ta.buffer);
            return a - b;
        });
    }, TypeError);
}

// Ensure detachment check doesn't choke on wrapped typed array.
if (typeof newGlobal === "function") {
    let ta = new Int32Array(3);
    let otherGlobal = newGlobal();
    otherGlobal.Int32Array.prototype.sort.call(ta, function(a, b) {
        return a - b;
    });
}

// Ensure detachment check works for wrapped typed arrays.
if (typeof newGlobal === "function" && typeof detachArrayBuffer === "function") {
    let detached = false;
    let ta = new Int32Array(3);
    let otherGlobal = newGlobal();
    assertThrowsInstanceOf(() => {
        otherGlobal.Int32Array.prototype.sort.call(ta, function(a,b) {
            assertEq(detached, false);
            detached = true;
            detachArrayBuffer(ta.buffer);
            return a - b;
        });
    }, otherGlobal.TypeError);
}

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
