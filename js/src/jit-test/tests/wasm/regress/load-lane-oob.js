// |jit-test| skip-if: !wasmSimdEnabled()

// Bug 1735128 - useless load should not be eliminated by Ion

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
`(module
   (memory 0)
   (func (export "f") (param i32)
     (v128.load64_lane 1 (local.get 0) (v128.const i32x4 0 0 0 0))
     drop))`)));
assertErrorMessage(() => ins.exports.f(100),
                   WebAssembly.RuntimeError,
                   /out of bounds/);

