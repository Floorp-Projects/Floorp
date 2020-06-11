// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker("new WeakRef({});");
