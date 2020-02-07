code = cacheEntry(`
  function f() {
    (function () {})
  };
  f()
  `);
evaluate(code, { saveIncrementalBytecode: { value: true } });
