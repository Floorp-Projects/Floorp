// |jit-test| --enable-weak-refs; skip-if: helperThreadCount() === 0
evalInWorker("new WeakRef({});");
