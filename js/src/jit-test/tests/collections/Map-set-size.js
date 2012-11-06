// map.set(k, v) increments the map size iff map didn't already have an entry for k.

var m = Map();
m.set('a', 0);
assertEq(m.size, 1);
m.set('a', 0);
assertEq(m.size, 1);
m.set('a', undefined);
assertEq(m.size, 1);

m.set('b', 2);
assertEq(m.size, 2);
m.set('a', 1);
assertEq(m.size, 2);
