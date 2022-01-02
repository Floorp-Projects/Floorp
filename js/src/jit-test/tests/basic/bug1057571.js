
test = (function () {
  function f() {
    [1,2,3,4,5];
  };
  return "var obj = { x : 2 };" + f.toString() + "; f()";
})();
evalWithCache(test, {});
function evalWithCache(code, ctx) {
  code = cacheEntry(code);
  ctx.global = newGlobal();
  ctx.isRunOnce = true;
  var res1 = evaluate(code, Object.create(ctx, {saveIncrementalBytecode: { value: true } }));
  var res2 = evaluate(code, Object.create(ctx, {loadBytecode: { value: true }, saveIncrementalBytecode: { value: true } }));
}
