// Bug 1747540 - two trap descriptors at the same address led to some confusion.

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (table 1 funcref)
  (memory 1)
  (func $test (export "test") (result i32)
    (call_indirect (i32.const 0))
    (i32.load (i32.const 0))
)
`)))
assertErrorMessage(() => ins.exports.test(),
                   WebAssembly.RuntimeError,
                   /indirect call to null/);

