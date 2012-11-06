// map.delete(k) decrements the map size iff an entry was actually removed.

var m = Map();
m.delete(3);
assertEq(m.size, 0);
m.set({}, 'ok');
m.set(Math, 'ok');
assertEq(m.size, 2);
m.delete({});
assertEq(m.size, 2);
m.delete(Math);
assertEq(m.size, 1);
m.delete(Math);
assertEq(m.size, 1);
