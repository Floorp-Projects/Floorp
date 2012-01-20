// Setting a Map key to undefined, or a missing argument, isn't the same as deleting it.

var m = new Map;
m.set(42, undefined);
assertEq(m.has(42), true);
assertEq(m.get(42), undefined);

m.set(42, "wrong");
m.set(42);
assertEq(m.has(42), true);
assertEq(m.get(42), undefined);

m.set();
assertEq(m.has(undefined), true);
assertEq(m.get(undefined), undefined);
