// NaN is equal to itself for the purpose of key lookups.

var m = new Map;
m.set(NaN, "ok");
assertEq(m.has(NaN), true);
assertEq(m.get(NaN), "ok");
assertEq(m.delete(NaN), true);
assertEq(m.has(NaN), false);
assertEq(m.get(NaN), undefined);

var s = new Set;
s.add(NaN);
assertEq(s.has(NaN), true);
assertEq(s.delete(NaN), true);
assertEq(s.has(NaN), false);
