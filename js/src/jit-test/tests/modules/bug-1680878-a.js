// |jit-test| error: SyntaxError
// Remove once Top-level await is enabled by default: Bug #1681046

r = parseModule(`
  for await (var x of this) {}
`);
r.declarationInstantiation();
r.evaluation();
