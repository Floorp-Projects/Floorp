// -0 is treated as the same key as +0.

var s = new Set;
s.add(-0);
assertEq(s.has(0), true);
assertEq(s.has(-0), true);

assertEq(s.delete(0), true);
assertEq(s.has(-0), false);
assertEq(s.has(0), false);

s.add(0);
assertEq(s.has(0), true);
assertEq(s.has(-0), true);
assertEq(s.delete(-0), true);
assertEq(s.has(-0), false);
assertEq(s.has(0), false);

var m = new Map;
m.set(-0, 'x');
assertEq(m.has(0), true);
assertEq(m.get(0), 'x');
assertEq(m.has(-0), true);
assertEq(m.get(-0), 'x');

assertEq(m.delete(0), true);
assertEq(m.has(-0), false);
assertEq(m.get(-0), undefined);
assertEq(m.has(0), false);
assertEq(m.get(0), undefined);

m.set(-0, 'x');
m.set(0, 'y');
assertEq(m.has(0), true);
assertEq(m.get(0), 'y');
assertEq(m.has(-0), true);
assertEq(m.get(-0), 'y');

assertEq(m.delete(-0), true);
assertEq(m.has(0), false);
assertEq(m.get(0), undefined);
assertEq(m.has(-0), false);
assertEq(m.get(-0), undefined);
