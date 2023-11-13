// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker(`
  a = new WeakMap
  b = Symbol
  a.set(b )
  c = b.hasInstance;
  a.get(c)
`);
