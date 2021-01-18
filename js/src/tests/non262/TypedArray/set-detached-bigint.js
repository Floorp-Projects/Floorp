let ta = new BigInt64Array(10);

let obj = {
  get length() {
    detachArrayBuffer(ta.buffer);
    return 1;
  },
  0: {
    valueOf() {
      return "huzzah!";
    }
  },
};

// Throws a SyntaxError, because "huzzah!" can't be parsed as a BigInt.
assertThrowsInstanceOf(() => ta.set(obj), SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
