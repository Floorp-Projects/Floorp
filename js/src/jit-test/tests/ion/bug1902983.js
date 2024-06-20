// |jit-test| --fast-warmup; --gc-zeal=21,100; skip-if: !wasmIsSupported()
let counter = 0;
function g() {
    counter++;
    const y = BigInt.asIntN(counter, -883678545n);
    const z = y >> y;
    BigInt.asUintN(2 ** counter, 883678545n);
    try { g(); } catch (e) { }
}
function f() {
    for (let i = 0; i < 5; i++) {
        for (let j = 0; j < 30; j++) { }
        Promise.allSettled().catch(e => null);
        counter = 0;
        g();
    }
}
const binary = wasmTextToBinary(`(module (import "m" "f" (func $f)) (func (export "test") (call $f)))`);
const mod = new WebAssembly.Module(binary);
const inst = new WebAssembly.Instance(mod, { m: { f: f } });
for (let i = 0; i < 100; i++) { }
for (let i = 0; i < 5; i++) {
    inst.exports.test();
}
