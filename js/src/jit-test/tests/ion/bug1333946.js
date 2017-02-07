// |jit-test| error: 42;

for (var x of [0]) {
  for (var i = 0; ; i++) {
    if (i === 20000)
      throw 42;
  }
}
