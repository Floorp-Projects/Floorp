// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker(`
    (new WeakMap).set(FakeDOMObject.prototype, this)
`);
