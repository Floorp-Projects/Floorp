let m = parseModule(`
  function f(x,y,z) {
    delete arguments[2];
    import.meta[2]
  }
  f(1,2,3)
`);
moduleLink(m);
moduleEvaluate(m);
