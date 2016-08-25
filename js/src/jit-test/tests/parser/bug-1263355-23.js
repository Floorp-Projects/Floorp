let m = parseModule(`
  const root = newGlobal();
  minorgc();
  root.eval();
`);
m.declarationInstantiation();
m.evaluation();
