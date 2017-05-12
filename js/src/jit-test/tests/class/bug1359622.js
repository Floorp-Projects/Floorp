setDiscardSource(true)
evaluate(`
  unescape(class get { static staticMethod() {} });
`);
