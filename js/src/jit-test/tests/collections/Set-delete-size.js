// set.delete(v) decrements set.size iff the set contained v.

var s = new Set();
for (var i = 0; i < 10; i++)
    s.add(i);

for (var i = 10; i > 0; i--) {
    assertEq(s.size, i);
    assertEq(s.delete(i), false);
    assertEq(s.size, i);
    assertEq(s.delete(i - 1), true);
    assertEq(s.size, i - 1);
    assertEq(s.delete(i - 1), false);
    assertEq(s.size, i - 1);
}
