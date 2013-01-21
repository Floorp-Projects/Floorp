// Binary: cache/js-dbg-32-47a8311cf0bb-linux
// Flags:
//
function g(x, n) {
  for (var i = 0; i < n; ++i) {
    x = {
      a: x
    };
  }
  return x;
}
try {
  d = g(0, 672);
} catch(exc1) {}
(function() {
  try {
    gczeal(2)(uneval(this))
  } catch(exc2) {}
})()
