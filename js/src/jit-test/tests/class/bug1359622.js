var g = newGlobal({ discardSource: true });
g.evaluate(`
  unescape(class get { static staticMethod() {} });
`);
