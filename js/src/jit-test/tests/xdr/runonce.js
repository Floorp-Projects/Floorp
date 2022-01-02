load(libdir + "asserts.js")

// Incremental XDR doesn't have run-once script restrictions.
evaluate(cacheEntry(""), { saveIncrementalBytecode: true });
evaluate(cacheEntry(""), { saveIncrementalBytecode: true, isRunOnce: false });
evaluate(cacheEntry(""), { saveIncrementalBytecode: true, isRunOnce: true });
