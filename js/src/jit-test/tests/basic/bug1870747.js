// |jit-test| --fast-warmup; --no-threads; --ion-check-range-analysis; --arm-hwcap=vfp
function foo(x) { return x % -1; }

with ({}) {}
for (var i = 0; i < 1000; i++) {
  foo(i);
}
