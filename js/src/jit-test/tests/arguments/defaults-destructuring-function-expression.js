let f = function(a, bIs, cIs, dIs, {b}={b: 3}, c=4, [d]=[5]) {
  assertEq(a, 1);
  assertEq(b, bIs);
  assertEq(c, cIs);
  assertEq(d, dIs);
};
assertEq(f.length, 4);
f(1, 3, 4, 5);
f(1, 42, 43, 44, {b: 42}, 43, [44]);
