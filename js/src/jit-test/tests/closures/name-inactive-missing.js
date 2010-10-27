// |jit-test| error: ReferenceError

function f(k) {
  function g(j) {
    return j + q;
  }
  return g;
}

g = f(10);
var ans = '';
for (var i = 0; i < 5; ++i) {
  ans += g(i) + ',';
}
