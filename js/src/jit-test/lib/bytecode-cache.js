
function evalWithCache(code, ctx) {
  ctx = ctx || {};
  ctx = Object.create(ctx, {
    fileName: { value: "evalWithCacheCode.js" },
    lineNumber: { value: 0 }
  });
  code = code instanceof Object ? code : cacheEntry(code);

  // We create a new global ...
  if (!("global" in ctx))
    ctx.global = newGlobal();

  // ... and by default enable compileAndGo.
  if (!("compileAndGo" in ctx))
    ctx.compileAndGo = true;

  // Fetch the verification function from the evaluation context.  This function
  // is used to assert the state of the script/function after each run of the
  // evaluate function.
  var checkAfter = ctx.checkAfter || function(ctx) {};

  // The generation counter is used to represent environment variations which
  // might cause the program to run differently, and thus to have a different
  // set of functions executed.
  ctx.global.generation = 0;
  var res1 = evaluate(code, Object.create(ctx, {saveBytecode: { value: true } }));
  checkAfter(ctx);

  ctx.global.generation = 1;
  var res2 = evaluate(code, Object.create(ctx, {loadBytecode: { value: true }, saveBytecode: { value: true } }));
  checkAfter(ctx);

  ctx.global.generation = 2;
  var res3 = evaluate(code, Object.create(ctx, {loadBytecode: { value: true } }));
  checkAfter(ctx);

  ctx.global.generation = 3;
  var res0 = evaluate(code, ctx);
  checkAfter(ctx);

  if (ctx.assertEqResult) {
    assertEq(res0, res1);
    assertEq(res0, res2);
    assertEq(res0, res3);
  }

  if (ctx.checkFrozen) {
    assertEq(Object.isFrozen(res0), Object.isFrozen(res1));
    assertEq(Object.isFrozen(res0), Object.isFrozen(res2));
    assertEq(Object.isFrozen(res0), Object.isFrozen(res3));
  }
}
