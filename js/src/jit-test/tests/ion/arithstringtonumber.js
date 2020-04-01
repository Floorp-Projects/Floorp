var g_array = [0, 1];

function g(p) {
  p = p - 0;
  return g_array[p - 0];
}

function f() {
  var q = 0;
  for (var i = 0; i < 10; ++i) {
    q += g("0x0");
    q += g("0x1");
  }
  return q;
}

for (var j = 0; j < 2; ++j) {
  assertEq(f(), 10);
}
