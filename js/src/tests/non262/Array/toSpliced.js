// |reftest| 

Object.defineProperty(Array.prototype, 0, {
  set() {
    throw "bad 0";
  },
});

Object.defineProperty(Array.prototype, 1, {
  set() {
    throw "bad 1";
  },
});

assertDeepEq([].toSpliced(0, 0, 1), [1]);

assertDeepEq([0].toSpliced(0, 0, 0), [0, 0]);
assertDeepEq([0].toSpliced(0, 0, 1), [1, 0]);
assertDeepEq([0].toSpliced(0, 1, 0), [0]);
assertDeepEq([0].toSpliced(0, 1, 1), [1]);
assertDeepEq([0].toSpliced(1, 0, 0), [0, 0]);
assertDeepEq([0].toSpliced(1, 0, 1), [0, 1]);
assertDeepEq([0].toSpliced(1, 1, 0), [0, 0]);
assertDeepEq([0].toSpliced(1, 1, 1), [0, 1]);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
