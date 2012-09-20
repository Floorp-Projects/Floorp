function f1(a, bIs, b=3) {
    assertEq(a, 1);
    assertEq(b, bIs);
}
assertEq(f1.length, 2);
f1(1, 3);
f1(1, 42, 42);
f1(1, 3, undefined);
function f2(a, bIs, cIs, b=3, c=4) {
    assertEq(a, 1);
    assertEq(b, bIs);
    assertEq(c, cIs);
}
assertEq(f2.length, 3);
f2(1, 3, 4);
f2(1, 42, 4, 42);
f2(1, 42, 43, 42, 43);
f2(1, 3, 4, undefined);
f2(1, 42, 4, 42, undefined);
f2(1, 3, 42, undefined, 42);
function f3(a, b, c=4) {
    assertEq(a, 1);
    assertEq(b, undefined);
    assertEq(c, 4);
}
f3(1);
