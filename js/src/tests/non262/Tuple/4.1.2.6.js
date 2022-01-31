// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

/*
4.1.2.6 [[Set]] ( P, Receiver )
The [[Set]] internal method of a Tuple exotic object takes arguments P and Receiver. It performs the following steps when called:

1. Assert: IsPropertyKey(P) is true.
2. Return false.
*/

// Setting properties should have no effect
var t = #[1];
t[4294967295] = "not a tuple element";
assertEq(t[4294967295], undefined);
assertEq(t.length, 1);
t[1] = 5;
assertEq(t.length, 1);

reportCompare(0, 0);
