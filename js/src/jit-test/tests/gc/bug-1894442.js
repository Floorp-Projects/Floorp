// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: helperThreadCount() === 0 || getBuildConfiguration("release_or_beta")
evalInWorker(`
  a = new WeakSet
  a.add(Symbol.hasInstance)
  gczeal(14)(0 .b)
`)
