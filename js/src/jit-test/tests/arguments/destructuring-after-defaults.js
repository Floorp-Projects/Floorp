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

function f2(a=(assertEq(a, undefined), assertEq(b, undefined),
               assertEq(c, undefined), assertEq(d, undefined), 1),
            [b],
            c=(assertEq(a, 1), assertEq(b, 42),
               assertEq(c, undefined), assertEq(d, undefined), 2),
            {d}) {
  assertEq(a, 1);
  assertEq(b, 42);
  assertEq(c, 2);
  assertEq(d, 43);
}
assertEq(f2.length, 0);
f2(undefined, [42], undefined, {d: 43});
