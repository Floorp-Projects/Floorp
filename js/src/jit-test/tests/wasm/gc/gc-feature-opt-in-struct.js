// |jit-test| skip-if: !wasmGcEnabled()
//
// Struct types are only available if we opt in, so test that.

let CURRENT_VERSION = 3;

new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in ${CURRENT_VERSION})
      (type (struct (field i32))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (type (struct (field i32))))`)),
                   WebAssembly.CompileError,
                   /Structure types not enabled/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func (struct.new 0)))`)),
                   WebAssembly.CompileError,
                   /unrecognized opcode/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func (struct.get 0 0)))`)),
                   WebAssembly.CompileError,
                   /unrecognized opcode/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func (struct.set 0 0)))`)),
                   WebAssembly.CompileError,
                   /unrecognized opcode/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func (struct.narrow anyref anyref)))`)),
                   WebAssembly.CompileError,
                   /unrecognized opcode/);

