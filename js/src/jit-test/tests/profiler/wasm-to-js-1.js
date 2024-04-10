// |jit-test| skip-if: !wasmIsSupported(); --fast-warmup
function sample() {
    enableGeckoProfiling();
    readGeckoProfilingStack();
    disableGeckoProfiling();
}
const text = `(module
    (import "m" "f" (func $f))
    (func (export "test")
    (call $f)
))`;
const bytes = wasmTextToBinary(text);
const mod = new WebAssembly.Module(bytes);
const imports = {"m": {"f": sample}};
const instance = new WebAssembly.Instance(mod, imports);
sample();
for (let i = 0; i < 5; i++) {
    gc(this, "shrinking");
    instance.exports.test();
}
