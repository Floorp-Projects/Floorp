function f() {
  var a = [{m: 0}, {m: 1}, {m: 2}, {m: 3}, {m: 4}];
  var b = [{}, {}, {}, {}, {}];
  for (var i = 0; i < a.length; i++) {
    a[i].m = function() { return 0; };
    b[i].m = function() { return 1; };
  }
  assertEq(false, a[0].m === a[1].m);
  assertEq(false, a[0].m === a[2].m);
  assertEq(false, a[0].m === a[3].m);
  assertEq(false, a[0].m === a[4].m);
  assertEq(false, b[0].m === b[1].m);
  assertEq(false, b[0].m === b[2].m);
  assertEq(false, b[0].m === b[3].m);
  assertEq(false, b[0].m === b[4].m);
}
f();
