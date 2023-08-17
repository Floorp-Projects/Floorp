// |jit-test| --fast-warmup; --no-threads

function foo(a,b) {
  a >> a
  b ^ b
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  foo(10n, -1n);
  try {
    foo(-2147483648n);
  } catch {}
}
