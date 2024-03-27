// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: getBuildConfiguration("release_or_beta")
var code = `
var m58 = new WeakMap;
var sym = Symbol();
m58.set(sym, ({ entry16: 0, length: 1 }));
function testCompacting() {
  gcslice(50000);
}
testCompacting(2, 100000, 50000);
`;
for (x = 0; x < 10000; ++x)
  evaluate(code);
