// Returning zero from the sort comparator...
let ta = new Int32Array([0, 1]).sort(() => 0);
assertEq(ta[0], 0);
assertEq(ta[1], 1);

// ... should give the same result as returning NaN.
let tb = new Int32Array([0, 1]).sort(() => NaN);
assertEq(tb[0], 0);
assertEq(tb[1], 1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
