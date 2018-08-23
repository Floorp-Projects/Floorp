// |jit-test| error: TypeError
let m = parseModule(`
  throw i => { return 5; }, m-1;
`);
instantiateModule(m);
evaluateModule(m);
