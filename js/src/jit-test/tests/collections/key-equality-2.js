// Number keys are distinct from string keys that would name the same property.

var s = new Set;

s.add(17);
assertEq(s.has("17"), false);
assertEq(s.has(17), true);
s.add("17");
assertEq(s.delete(17), true);
assertEq(s.has("17"), true);
assertEq(s.has(17), false);
