// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var tup = #[1, 2, 10n ** 100n, Tuple(5, 6)];

assertEq(tup[0], 1);
assertEq(tup[1], 2);
assertEq(tup[2], 10n ** 100n);
assertEq(typeof tup[3], "tuple");
assertEq(tup[3][0], 5);
assertEq(tup[3][1], 6);

var err;
try {
    tup = #[1, 2, 3,
      4, 5,
      7, {}, 8];
} catch (e) { err = e }

assertEq(err.lineNumber, 16);
assertEq(err.columnNumber, 10);

if (typeof reportCompare === "function") reportCompare(0, 0);
