function f1(a, bIs, cIs, dIs, b=3, {c}={c: 4}, [d]=[5]) {
    assertEq(a, 1);
    assertEq(b, bIs);
    assertEq(c, cIs);
    assertEq(d, dIs);
}
assertEq(f1.length, 4);
f1(1, 3, 4, 5);
f1(1, 42, 4, 5, 42);
f1(1, 42, 43, 5, 42, {c: 43});
f1(1, 42, 43, 44, 42, {c: 43}, [44]);
f1(1, 3, 4, 5, undefined);
f1(1, 42, 4, 5, 42, undefined);
f1(1, 3, 42, 5, undefined, {c: 42});
f1(1, 3, 4, 42, undefined, undefined, [42]);

function f2(a, bIs, cIs, dIs, eIs, {b}={b: 3}, [c]=[b], d=c, {ee: e}={ee: d}) {
  assertEq(a, 1);
  assertEq(b, bIs);
  assertEq(c, cIs);
  assertEq(d, dIs);
  assertEq(e, eIs);
}
assertEq(f2.length, 5);
f2(1, 3, 3, 3, 3);
f2(1, 42, 42, 42, 42, {b: 42});
f2(1, 42, 43, 43, 43, {b: 42}, [43]);
f2(1, 42, 43, 44, 44, {b: 42}, [43], 44);
f2(1, 42, 43, 44, 45, {b: 42}, [43], 44, {ee: 45});
