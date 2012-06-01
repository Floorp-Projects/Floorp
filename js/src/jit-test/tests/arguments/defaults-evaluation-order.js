function f1(a, bIs, cIs, dIs, b=a, c=d, d=5) {
    assertEq(a, 1);
    assertEq(b, bIs);
    assertEq(c, cIs);
    assertEq(d, dIs);
}
f1(1, 1, undefined, 5);
f1(1, 42, undefined, 5, 42);
f1(1, 42, 43, 5, 42, 43);
f1(1, 42, 43, 44, 42, 43, 44);
function f2(a=[]) { return a; }
assertEq(f2() !== f2(), true);
function f3(a=function () {}) { return a; }
assertEq(f3() !== f3(), true);
function f4(a=Date) { return a; }
assertEq(f4(), Date);
Date = 0;
assertEq(f4(), 0);
function f5(x=FAIL()) {};  // don't throw
var n = 0;
function f6(a=n++) {}
assertEq(n, 0);
function f7([a, b], A=a, B=b) {
    assertEq(A, a);
    assertEq(B, b);
}
f7([0, 1]);
