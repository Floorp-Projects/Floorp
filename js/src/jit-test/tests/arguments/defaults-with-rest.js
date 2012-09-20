load(libdir + "eqArrayHelper.js");

function f1(a, bIs, b=3, ...rest) {
    assertEq(a, 1);
    assertEq(bIs, b);
    assertEqArray(rest, []);
}
assertEq(f1.length, 2);
f1(1, 3);
f1(1, 42, 42);
function f2(a=rest, ...rest) {
    assertEq(a, undefined);
}
f2();
function f3(a=rest, ...rest) {
    assertEq(a, 1);
    assertEqArray(rest, [2, 3, 4]);
}
f3(1, 2, 3, 4);
function f4(a=42, ...f)  {
    assertEq(typeof f, "function");
    function f() {}
}
f4()
