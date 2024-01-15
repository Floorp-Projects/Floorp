// |jit-test| --no-threads; --fast-warmup

function f(x) {
  Math.fround(function () { x; });
}
for (let i = 0; i < 30; i++) {
  f(Math.fround(1));
}
