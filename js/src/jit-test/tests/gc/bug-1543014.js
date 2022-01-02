// |jit-test| skip-if: helperThreadCount() === 0
gczeal(0);
evalInWorker(`
  var sym4 = Symbol.match;
  function basicSweeping() {};
  var wm1 = new WeakMap();
  wm1.set(basicSweeping, sym4);
  startgc(100000, 'shrinking');
`);
gczeal(2);
var d1 = newGlobal({});
