function f(x) {
  assertEq(x.charCodeAt(1), 0x62);
  assertEq(x.charAt(1), "b");
  assertEq(x[1], "b");
}

function g() {
  for (var i = 0; i < 100; i++) {
    f("abc");
  }
}

for (var j = 0; j < 10; j++) {
  g();
}
