// Bug 770954.

gczeal(4);
var s = new Set;
s.add(-0);
s.add(0);
assertEq(s.delete(-0), true);
assertEq(s.has(0), (true  ));
assertEq(s.has(-0), false);
var m = new Map;
m.set(-0, 'x');
assertEq(m.has(0), false);
assertEq(m.get(0), undefined);
assertEq(m.has(-0), true);
assertEq(m.delete(-0), true);
