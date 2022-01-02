// mapiter.next() returns a fresh array each time.

load(libdir + "iteration.js");

var map = new Map([['a', 1], ['b', 2]]);
var iter = map[Symbol.iterator]();
var a = iter.next(), b = iter.next();
assertIteratorResult(a, ['a', 1], false);
assertIteratorResult(b, ['b', 2], false);
assertEq(a.value !== b.value, true);
var a1 = map[Symbol.iterator]();
assertIteratorNext(a1, ['a', 1]);
assertEq(a.value !== a1.value, true);
