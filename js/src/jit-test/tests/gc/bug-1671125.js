// |jit-test| skip-if: helperThreadCount() === 0
verifyprebarriers()
evalInWorker(`
  Object.defineProperty(this, "x", {});
`);
