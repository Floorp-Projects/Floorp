// |jit-test| skip-if: helperThreadCount() === 0

evalInWorker(`
  verifyprebarriers();
  Number + 1 >> 2;
`)
