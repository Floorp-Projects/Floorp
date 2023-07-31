// |reftest|

var ta = new Int32Array([3, 2, 1]);

detachArrayBuffer(ta.buffer);

assertThrowsInstanceOf(() => ta.toSorted(), TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
