load(libdir + 'bytecode-cache.js');
var test = "";
gczeal(0);

// Check that a GC can relazify decoded functions.
//
// Generations 0 and 3 are executed from the source, thus f is not executed yet.
// Generations 1 and 2 are decoded, thus we recorded the delazified f function.
test = `
  function f() { return 1; };
  assertEq(isLazyFunction(f), generation == 0 || generation == 3);
  f();
  expect = isRelazifiableFunction(f);
  assertEq(isLazyFunction(f), false);
`;
evalWithCache(test, {
  checkAfter: function (ctx) {
    gc(ctx.global.f, "shrinking"); // relazify f, if possible.
    evaluate("assertEq(isLazyFunction(f), expect);", ctx);
  }
});

evalWithCache(test, {
  incremental: true,
  checkAfter: function (ctx) {
    gc(ctx.global.f, "shrinking"); // relazify f, if possible.
    evaluate("assertEq(isLazyFunction(f), expect);", ctx);
  }
});
