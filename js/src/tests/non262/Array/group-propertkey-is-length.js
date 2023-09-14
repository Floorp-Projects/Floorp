var array = [0];

var grouped = Object.groupBy(array, () => "length");

assertDeepEq(grouped, Object.create(null, {
  length: {
    value: [0],
    writable: true,
    enumerable: true,
    configurable: true,
  },
}));

if (typeof reportCompare === "function")
  reportCompare(0, 0);
