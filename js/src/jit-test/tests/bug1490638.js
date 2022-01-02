// |jit-test| allow-overrecursed; skip-if: helperThreadCount() === 0

evalInWorker(`
  function f() {
    f.apply([], new Array(20000));
  }
  f()
`);
