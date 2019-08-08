evaluate(`
  for (var i = 0; i < 2000; i++) {
    Array(Math, {});
    bailout();
  }
`);
