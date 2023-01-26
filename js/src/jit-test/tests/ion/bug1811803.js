// |jit-test| --fuzzing-safe; --ion-offthread-compile=off; --fast-warmup

a = {}
for (b = 0; b < 100; ++b)
  a['x' + b] = 'x' + b;
