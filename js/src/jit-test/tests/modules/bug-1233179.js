let c = parseModule(`
  function a(x) { return x; }
  function b(x) { return i<40; }
  function d(x) { return x + 3; }
`);
getLcovInfo();
