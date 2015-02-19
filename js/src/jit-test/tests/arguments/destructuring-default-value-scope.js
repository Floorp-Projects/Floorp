var a = 10;
function f1(a,
            [b=(assertEq(a, 1), a=2, 42)],
            {c:c=(assertEq(a, 2), a=3, 43)}) {
  assertEq(a, 3);
  assertEq(b, 42);
  assertEq(c, 43);
}
f1(1, [], {});
assertEq(a, 10);
