new FinalizationRegistry(a => {});
b = newGlobal({newCompartment: true});
b.eval(`
  c = {};
  d = new WeakRef(c);
`);
nukeCCW(b.d);
