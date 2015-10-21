
test = (function () {
  function f() {};
  return "var obj = { x : 2 };" + f.toSource() + (4);
})();
evalWithCache(test, {});
function evalWithCache(code, ctx) {
  code = cacheEntry(code);
  ctx.global = newGlobal({ cloneSingletons: true });
  var res1 = evaluate(code, Object.create(ctx, {saveBytecode: { value: true } }));
}
if (typeof assertThrowsInstanceOf === 'undefined') {
    var assertThrowsInstanceOf = function assertThrowsInstanceOf(f, ctor, msg) {};
}
evaluate('evaluate(\'assertThrowsInstanceOf(function () {}, ["jak", "ms"]);\', { noScriptRval : true, isRunOnce: true  })');
