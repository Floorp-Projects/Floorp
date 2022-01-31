// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
10.2 ECMAScript Function Objects
...
All ECMAScript function objects have the [[Call]] internal method defined here. ECMAScript functions that are also constructors in addition have the [[Construct]] internal method.
*/

assertEq(Tuple.call(), #[]);
assertEq(Tuple.call(undefined), #[]);
assertEq(Tuple.call(undefined, 1), #[1]);
assertEq(Tuple.call(2, 1), #[1]);
assertEq(Tuple.call(#[], 1), #[1]);
assertEq(Tuple.call(undefined, 1, 2, 3), #[1,2,3]);
assertEq(Tuple.call(6, 1, 2, 3), #[1,2,3]);
assertEq(Tuple.call(#[1], 1, 2, 3), #[1,2,3]);

reportCompare(0, 0);
