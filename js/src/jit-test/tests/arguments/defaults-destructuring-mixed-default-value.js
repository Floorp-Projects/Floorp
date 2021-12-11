function f1(a=1,
            [b, c=(assertEq(a, 2), a=3, 42)]=[(assertEq(a, 1), a=2, 43)],
            {d, e:e=(assertEq(a, 4), a=5, 44)}={d: (assertEq(a, 3), a=4, 45)},
            f=(assertEq(a, 5), a=6, 46)) {
  assertEq(a, 6);
  assertEq(b, 43);
  assertEq(c, 42);
  assertEq(d, 45);
  assertEq(e, 44);
  assertEq(f, 46);
}
assertEq(f1.length, 0);
f1();

function f2(a=1,
            [b, c=assertEq(false)]=[(assertEq(a, 1), a=2, 42), (assertEq(a, 2), a=3, 43)],
            {d, e:e=assertEq(false)}={d: (assertEq(a, 3), a=4, 44), e: (assertEq(a, 4), a=5, 45)},
            f=(assertEq(a, 5), a=6, 46)) {
  assertEq(a, 6);
  assertEq(b, 42);
  assertEq(c, 43);
  assertEq(d, 44);
  assertEq(e, 45);
  assertEq(f, 46);
}
assertEq(f2.length, 0);
f2();

function f3(a=1,
            [b, c=(assertEq(a, 1), a=2, 42)]=[assertEq(false)],
            {d, e:e=(assertEq(a, 2), a=3, 43)}={d: assertEq(false)},
            f=(assertEq(a, 3), a=4, 44)) {
  assertEq(a, 4);
  assertEq(b, 8);
  assertEq(c, 42);
  assertEq(d, 9);
  assertEq(e, 43);
  assertEq(f, 44);
}
assertEq(f3.length, 0);
f3(undefined, [8], {d: 9});

function f4(a=1,
            [b, c=assertEq(false)]=[assertEq(false), assertEq(false)],
            {d, e:e=assertEq(false)}={d: assertEq(false), e: assertEq(false)},
            f=(assertEq(a, 1), a=2, 42)) {
  assertEq(a, 2);
  assertEq(b, 8);
  assertEq(c, 9);
  assertEq(d, 10);
  assertEq(e, 11);
  assertEq(f, 42);
}
assertEq(f4.length, 0);
f4(undefined, [8, 9], {d: 10, e: 11});
