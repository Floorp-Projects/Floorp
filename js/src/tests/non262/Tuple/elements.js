// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var tup = Tuple(1, 2, 10n ** 100n, Tuple(5, 6));

assertEq(tup[0], 1);
assertEq(tup[1], 2);
assertEq(tup[2], 10n ** 100n);
assertEq(typeof tup[3], "tuple");
assertEq(tup[3][0], 5);
assertEq(tup[3][1], 6);

assertEq(Object(tup)[0], 1);

assertEq(tup.length, 4);
assertEq(Object(tup).length, 4);

assertEq("0" in Object(tup), true);

if (typeof reportCompare === "function") reportCompare(0, 0);
