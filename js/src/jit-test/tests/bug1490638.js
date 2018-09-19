// |jit-test| allow-overrecursed
if (helperThreadCount() == 0)
    quit();

evalInWorker(`
  function f() {
    f.apply([], new Array(20000));
  }
  f()
`);
