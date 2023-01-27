function* gen(z) {
  try {
    yield 1;
  } finally {
    for (let i = 0; i < 10; i++) {}
    var x = z + 1;
  }
}

for (let i = 0; i < 20; i++) {
  let g = gen(1);
  g.next();
  g.return();
}
