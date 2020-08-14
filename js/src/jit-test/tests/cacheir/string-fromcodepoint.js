function f() {
  for (var i = 0x10F000; i <= 0x10FFFF + 1; ++i) {
    String.fromCodePoint(i);
  }
}

for (var i = 0; i < 2; ++i) {
  var err;
  try {
    f();
  } catch (e) {
    err = e;
  }
  assertEq(err instanceof RangeError, true);
}
