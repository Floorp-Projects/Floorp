function lfEvalInCache(lfCode, lfIncremental = false, lfRunOnce = false) {
  let ctx = {};
  let code = cacheEntry(lfCode);
  ctx_save = Object.create(ctx, { saveIncrementalBytecode: { value: true } });
  try { evaluate(code, ctx_save); } catch(exc) {}
  try { evaluate(code, Object.create(ctx_save, { loadBytecode: { value: true } })); } catch(exc) {}
}
lfEvalInCache(`
  function q() {}
  Object.freeze(this);
`, false, true);
