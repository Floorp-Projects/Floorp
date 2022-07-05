let m = parseModule(`
  const root = newGlobal();
  minorgc();
  root.eval();
`);
moduleLink(m);
moduleEvaluate(m);
