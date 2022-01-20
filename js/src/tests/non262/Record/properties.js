// |reftest| skip-if(!this.hasOwnProperty("Record"))

var rec = Record({
  x: 1,
  4: 2,
  z: 10n ** 100n,
  w: Record({}),
  n: Tuple(),
  [Symbol.iterator]: 4,
});

assertEq(rec.x, 1);
assertEq(rec[4], 2);
assertEq(rec.z, 10n ** 100n);
assertEq(typeof rec.w, "record");
assertEq(typeof rec.n, "tuple");
assertEq(rec[Symbol.iterator], undefined);
assertEq(rec.hasOwnProperty, undefined);

if (typeof reportCompare === "function") reportCompare(0, 0);
