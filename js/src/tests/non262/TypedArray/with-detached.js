// |reftest| shell-option(--enable-change-array-by-copy) skip-if(!Int32Array.prototype.with)

var ta = new Int32Array([3, 2, 1]);

detachArrayBuffer(ta.buffer);

assertThrowsInstanceOf(() => ta.with(0, 0), TypeError);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
