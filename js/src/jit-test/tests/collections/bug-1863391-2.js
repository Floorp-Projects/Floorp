// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: helperThreadCount() === 0 || getBuildConfiguration("release_or_beta")
evalInWorker(`
  a = new WeakMap
  b = Symbol.hasInstance;
  a.set(b, 2);
`);
