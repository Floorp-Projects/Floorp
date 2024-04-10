// |jit-test| skip-if: !wasmIsSupported()
// Ensure readGeckoProfilingStack finds at least 1 Wasm frame on the stack.
function calledFromWasm() {
    let frames = readGeckoProfilingStack().flat();
    assertEq(frames.filter(f => f.kind === "wasm").length >= 1, true);
}
enableGeckoProfiling();
const text = `(module
    (import "m" "f" (func $f))
    (func (export "test")
    (call $f)
))`;
const bytes = wasmTextToBinary(text);
const mod = new WebAssembly.Module(bytes);
const imports = {"m": {"f": calledFromWasm}};
const instance = new WebAssembly.Instance(mod, imports);
for (let i = 0; i < 150; i++) {
    instance.exports.test();
}
