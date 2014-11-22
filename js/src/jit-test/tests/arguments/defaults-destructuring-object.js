function f1(a, bIs, cIs, {b}={b: 3}, {cc: c}={cc: 4}) {
    assertEq(a, 1);
    assertEq(b, bIs);
    assertEq(c, cIs);
}
assertEq(f1.length, 3);
f1(1, 3, 4);
f1(1, 42, 4, {b: 42});
f1(1, 42, 4, {b: 42}, undefined);
f1(1, 42, 43, {b: 42}, {cc: 43});
f1(1, 3, 4, undefined);
f1(1, 3, 4, undefined, undefined);
f1(1, 3, 43, undefined, {cc: 43});

function f2(a, bIs, cIs, {b}={}, {cc: c}={}) {
    assertEq(a, 1);
    assertEq(b, bIs);
    assertEq(c, cIs);
}
assertEq(f2.length, 3);
f2(1, undefined, undefined);
f2(1, 42, undefined, {b: 42});
f2(1, 42, undefined, {b: 42}, undefined);
f2(1, 42, 43, {b: 42}, {cc: 43});
f2(1, undefined, undefined, undefined);
f2(1, undefined, undefined, undefined, undefined);
f2(1, undefined, 43, undefined, {cc: 43});
