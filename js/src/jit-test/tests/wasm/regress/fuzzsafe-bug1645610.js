// |jit-test| --fuzzing-safe; skip-if: this.getErrorNotes
//
// The meaning of the line above is that the test case is to be run only with
// --fuzzing-safe and only if that setting (or similar settings) is effectful.

WebAssembly.Module.exports(new WebAssembly.Module(wasmTextToBinary(`
(module
  (func (;0;))
  (func (;1;))
  (func (;2;))
  (func (;3;) (result i32)
    i32.const 42)
  (export "memo" (func 3))
  (export "main" (func 3)))
`)));
