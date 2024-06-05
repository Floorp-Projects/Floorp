// |jit-test| --fast-warmup; --no-threads; skip-if: !wasmIsSupported()
function f1() {
  Promise.allSettled().catch(e => null);
  do {
    f2(10n, -1n);
    try {
      f2(-2147483648n);
    } catch {}
  } while (!inIon());
}
function f2(x, y) {
  const z = x >> x;
  z <= z ? z : z;
  y ^ y;
}
const binary = wasmTextToBinary(`
  (module
    (import "m" "f" (func $f))
    (func (export "test")
      (call $f)
    )
  )
`);
const mod = new WebAssembly.Module(binary);
const inst = new WebAssembly.Instance(mod, {"m": {"f": f1}});
for (let i = 0; i < 6; i++) {
  inst.exports.test();
}
