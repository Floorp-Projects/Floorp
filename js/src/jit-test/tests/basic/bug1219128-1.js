evaluate(`
  x = evalcx("lazy");
  oomTest(function () {
    x.start("1");
  });
`);
