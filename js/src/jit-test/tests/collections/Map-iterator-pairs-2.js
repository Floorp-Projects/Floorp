// mapiter.next() returns a fresh array each time.

var map = Map([['a', 1], ['b', 2]]);
var iter = map.iterator();
var a = iter.next(), b = iter.next();
assertEq(a !== b, true);
assertEq(a[0], 'a');
assertEq(b[0], 'b');
var a1 = map.iterator().next();
assertEq(a !== a1, true);
