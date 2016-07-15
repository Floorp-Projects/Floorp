if (helperThreadCount() === 0)
    quit();
evalInWorker(`
    (new WeakMap).set(FakeDOMObject.prototype, this)
`);
