// |reftest| shell-option(--enable-array-grouping) skip-if(!Array.prototype.group)

var array = [0];

var grouped = array.group(() => "length");

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
