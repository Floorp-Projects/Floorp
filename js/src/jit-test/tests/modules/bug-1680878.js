// |jit-test| error: TypeError

r = parseModule(`
  for await (var x of this) {}
`);
moduleLink(r);
moduleEvaluate(r);
