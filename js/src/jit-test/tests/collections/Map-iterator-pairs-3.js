// Modifying an array returned by mapiter.next() does not modify the Map.

var map = Map([['a', 1]]);
var pair = map.iterator().next();
assertEq(pair[0], 'a');
pair[0] = 'b';
pair[1] = 2;
assertEq(pair[0], 'b');
assertEq(pair[1], 2);
assertEq(map.get('a'), 1);
assertEq(map.has('b'), false);
assertEq(map.size, 1);
