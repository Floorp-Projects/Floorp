gczeal(4);
evalWithCache(`
  var obj = { a: 1, b: 2 };
  obj.a++;
  assertEq(obj.a, 2);
`);
function evalWithCache(code) {
  code = cacheEntry(code);
  ctx_save = Object.create({}, { saveIncrementalBytecode: { value: true } });
  var res1 = evaluate(code, ctx_save);
  var res2 = evaluate(code, Object.create(ctx_save, {}));
}
