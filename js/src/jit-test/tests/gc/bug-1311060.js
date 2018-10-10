// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker(`schedulegc("s1");`);
