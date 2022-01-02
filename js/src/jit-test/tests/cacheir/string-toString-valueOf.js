function simple(str) {
  assertEq(str.toString(), "abc");
  assertEq(str.valueOf(), "abc");
}

function obj(str) {
  var obj = new String(str);
  assertEq(obj.toString(), "xyz");
  assertEq(obj.valueOf(), "xyz");
}

function mixed() {
  for (var v of ["abc", new String("abc")]) {
    assertEq(v.toString(), "abc");
    assertEq(v.valueOf(), "abc");
  }
}

for (var i = 0; i < 100; i++) {
  simple("abc");
  obj("xyz");
  mixed();
}