// |jit-test| --blinterp-eager; --ion-warmup-threshold=0; --fast-warmup; --no-threads

function bar(x, y) {
  return ((Math.fround(x) && Math.fround(y)) >>> 0) + y & x | 0 + undef();
};

function foo(f, inputs) {
  for (var j = 0; j < inputs.length; ++j)
    for (var k = 0; k < inputs.length; ++k)
      try {
        f(inputs[j], inputs[k]);
      } catch {}

}

foo(bar, [1, 1, 1, 1, 1, 1, 1, 1, -0x080000001, -0x0ffffffff]);
