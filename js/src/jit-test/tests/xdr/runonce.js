load(libdir + "asserts.js")

// JS::EncodeScript cannot be use for run-once scripts.
evaluate(cacheEntry(""), { saveBytecode: true });
evaluate(cacheEntry(""), { saveBytecode: true, isRunOnce: false })
assertErrorMessage(() => {
  evaluate(cacheEntry(""), { saveBytecode: true, isRunOnce: true })
}, Error, "run-once script are not supported by XDR");

// Incremental XDR doesn't have any of these restrictions.
evaluate(cacheEntry(""), { saveIncrementalBytecode: true });
evaluate(cacheEntry(""), { saveIncrementalBytecode: true, isRunOnce: false });
evaluate(cacheEntry(""), { saveIncrementalBytecode: true, isRunOnce: true });
