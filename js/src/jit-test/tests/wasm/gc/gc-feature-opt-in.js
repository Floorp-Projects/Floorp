if (!wasmGcEnabled()) {
    quit(0);
}

// Encoding.  If the section is present it must be first.

var bad_order =
    new Uint8Array([0x00, 0x61, 0x73, 0x6d,
                    0x01, 0x00, 0x00, 0x00,

                    0x01,                   // Type section
                    0x01,                   // Section size
                    0x00,                   // Zero types

                    0x2a,                   // GcFeatureOptIn section
                    0x01,                   // Section size
                    0x01]);                 // Version

assertErrorMessage(() => new WebAssembly.Module(bad_order),
                   WebAssembly.CompileError,
                   /expected custom section/);

// Version numbers.  Version 1 is good, version 2 is bad.

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in 2))`)),
                   WebAssembly.CompileError,
                   /unsupported version of the gc feature/);

// Struct types are only available if we opt in.

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1)
      (type (struct (field i32))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (type (struct (field i32))))`)),
                   WebAssembly.CompileError,
                   /Structure types not enabled/);

// Parameters of ref type are only available if we opt in.

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1)
      (type (func (param anyref))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (type (func (param anyref))))`)),
                   WebAssembly.CompileError,
                   /reference types not enabled/);

// Ditto returns

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1)
      (type (func (result anyref))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (type (func (result anyref))))`)),
                   WebAssembly.CompileError,
                   /reference types not enabled/);

// Ditto locals

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1)
      (func (result i32)
       (local anyref)
       (i32.const 0)))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func (result i32)
       (local anyref)
       (i32.const 0)))`)),
                   WebAssembly.CompileError,
                   /reference types not enabled/);

// Ditto globals

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1)
      (global (mut anyref) (ref.null anyref)))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (global (mut anyref) (ref.null anyref)))`)),
                   WebAssembly.CompileError,
                   /reference types not enabled/);

// Ref instructions are only available if we opt in.
//
// When testing these we need to avoid struct types or parameters, locals,
// returns, or globals of ref type, or guards on those will preempt the guards
// on the instructions.

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func (ref.null anyref)))`)),
                   WebAssembly.CompileError,
                   /unrecognized opcode/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func ref.is_null))`)),
                   WebAssembly.CompileError,
                   /unrecognized opcode/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func ref.eq))`)),
                   WebAssembly.CompileError,
                   /unrecognized opcode/);

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
