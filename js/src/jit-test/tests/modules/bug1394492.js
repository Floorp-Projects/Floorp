// |jit-test| error: NaN
let m = parseModule(`
  throw i => { return 5; }, m-1;
`);
m.declarationInstantiation();
m.evaluation();
