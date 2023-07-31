// |reftest| 

Object.defineProperty(Array.prototype, 0, {
  set() {
    throw "bad";
  },
});

// Single element case.
assertDeepEq([0].with(0, 1), [1]);

// More than one element.
assertDeepEq([1, 2].with(0, 3), [3, 2]);
assertDeepEq([1, 2].with(1, 3), [1, 3]);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
