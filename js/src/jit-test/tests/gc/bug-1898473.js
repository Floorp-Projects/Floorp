// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker(`
  new FinalizationRegistry(Set).register(newGlobal())
  gc()
`);
