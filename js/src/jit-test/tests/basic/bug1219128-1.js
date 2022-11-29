// |jit-test| skip-if: !('oomTest' in this)

evaluate(`
  x = evalcx("lazy");
  oomTest(function () {
    x.start("1");
  });
`);
