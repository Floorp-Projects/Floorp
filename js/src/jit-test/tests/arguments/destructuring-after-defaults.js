load(libdir + "asserts.js");

function f1(a, bIs, cIs, dIs, b=1, [c], {d}) {
    assertEq(a, 1);
    assertEq(b, bIs);
    assertEq(c, cIs);
    assertEq(d, dIs);
}
assertEq(f1.length, 4);
f1(1, 1, 42, 43, undefined, [42], {d: 43});
f1(1, 42, 43, 44, 42, [43], {d: 44});
assertThrowsInstanceOf(function () {
  f1(1, 1, 1, 1);
}, TypeError);
