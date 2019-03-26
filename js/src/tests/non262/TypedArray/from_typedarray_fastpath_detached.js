// Ensure the fast-path when TypedArray.from is called with a TypedArray still
// checks for detached buffers.

var ta = new Int32Array(4);
detachArrayBuffer(ta.buffer);

assertThrowsInstanceOf(() => Int32Array.from(ta), TypeError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
