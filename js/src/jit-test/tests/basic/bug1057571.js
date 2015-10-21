
test = (function () {
  function f() {
    [1,2,3,4,5];
  };
  return "var obj = { x : 2 };" + f.toSource() + "; f()";
})();
evalWithCache(test, {});
function evalWithCache(code, ctx) {
  code = cacheEntry(code);
  ctx.global = newGlobal({ cloneSingletons: true });
  ctx.isRunOnce = true;
  var res1 = evaluate(code, Object.create(ctx, {saveBytecode: { value: true } }));
  var res2 = evaluate(code, Object.create(ctx, {loadBytecode: { value: true }, saveBytecode: { value: true } }));
}
