// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

let tup = #[1, 2, 3];

assertEq(tup.length, 3);
assertEq(tup[0], 1);
assertEq(tup[1], 2);
assertEq(tup[2], 3);

let arr = [3, 4, 5];
tup = #[1, 2, ...arr, 6, ...arr];

assertEq(tup.length, 9);
assertEq(tup[0], 1);
assertEq(tup[1], 2);
assertEq(tup[2], 3);
assertEq(tup[3], 4);
assertEq(tup[4], 5);
assertEq(tup[5], 6);
assertEq(tup[6], 3);
assertEq(tup[7], 4);
assertEq(tup[8], 5);

tup = #[(() => 1)()];

assertEq(tup.length, 1);
assertEq(tup[0], 1);

tup = #[];
assertEq(tup.length, 0);

if (typeof reportCompare === "function") reportCompare(0, 0);
