// |jit-test| --fast-warmup; --no-threads
function h(x) {
  var z = +x;
  assertEq(1, 1);
  0.1 ** z;
  bailout();
}
function g1(x) {
  for (var i = 0; i < 10; i++) {
    h(x >> 1);
  }
}
function g2(x) {
  for (var i = 0; i < 10; i++) {
    h(x);
  }
}
function f() {
  for (var i = 0; i < 5; i++) {
    g1(1);
    g2(0.1);
  }
}
f();
