// |jit-test| skip-if: helperThreadCount() === 0
evalInWorker(`
var sym4 = Symbol.match;
function test(makeNonArray) {}
function basicSweeping() {}
var wm1 = new WeakMap();
wm1.set(basicSweeping, sym4);
startgc(100000, 'shrinking');
`);
