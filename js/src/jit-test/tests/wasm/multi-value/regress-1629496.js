let bytes = wasmTextToBinary(`
  (module
    (func $f (param) (result i32 i32)
      (local i32)
      (loop)
      (i32.const 0)
      (i32.const 1)))`);

new WebAssembly.Instance(new WebAssembly.Module(bytes));
