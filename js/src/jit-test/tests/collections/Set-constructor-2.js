// The Set constructor can take an argument that is an array.

var s = Set([]);
assertEq(s.size, 0);
assertEq(s.has(undefined), false);

s = Set(["one", "two", "three"]);
assertEq(s.size, 3);
assertEq(s.has("one"), true);
assertEq(s.has("eleventeen"), false);

var a = [{}, {}, {}];
s = Set(a);
assertEq(s.size, 3);
for (let obj of a)
    assertEq(s.has(obj), true);
assertEq(s.has({}), false);
assertEq(s.has("three"), false);
