// |reftest| shell-option(--enable-change-array-by-copy) skip-if(!Int32Array.prototype.toReversed)

var ta = new Int32Array([3, 2, 1]);

detachArrayBuffer(ta.buffer);

assertThrowsInstanceOf(() => ta.toReversed(), TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
