// |jit-test| --enable-top-level-await; error: TypeError

r = parseModule(`
  for await (var x of this) {}
`);
r.declarationInstantiation();
r.evaluation();
