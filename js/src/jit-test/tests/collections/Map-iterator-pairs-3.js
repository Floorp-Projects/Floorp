// Modifying an array returned by mapiter.next() does not modify the Map.

load(libdir + "iteration.js");

var map = Map([['a', 1]]);
var res = map[std_iterator]().next();
assertIteratorResult(res, ['a', 1], false);
res.value[0] = 'b';
res.value[1] = 2;
assertIteratorResult(res, ['b', 2], false);
assertEq(map.get('a'), 1);
assertEq(map.has('b'), false);
assertEq(map.size, 1);
