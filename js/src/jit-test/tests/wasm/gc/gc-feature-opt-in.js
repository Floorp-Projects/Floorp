// |jit-test| skip-if: !wasmReftypesEnabled()
//
// Also see gc-feature-opt-in-struct.js for tests that use the struct feature.

// Version numbers

let CURRENT_VERSION = 3;
let SOME_OLDER_INCOMPATIBLE_VERSION = CURRENT_VERSION - 1;  // v1 incompatible with v2, v2 with v3
let SOME_FUTURE_VERSION = CURRENT_VERSION + 1;              // ok for now

// Encoding.  If the section is present it must be first.

var bad_order =
    new Uint8Array([0x00, 0x61, 0x73, 0x6d,
                    0x01, 0x00, 0x00, 0x00,

                    0x01,                   // Type section
                    0x01,                   // Section size
                    0x00,                   // Zero types

                    0x2a,                   // GcFeatureOptIn section
                    0x01,                   // Section size
                    CURRENT_VERSION]);      // Version

assertErrorMessage(() => new WebAssembly.Module(bad_order),
                   WebAssembly.CompileError,
                   /expected custom section/);

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in ${CURRENT_VERSION}))`));

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in ${CURRENT_VERSION}))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in ${SOME_OLDER_INCOMPATIBLE_VERSION}))`)),
                   WebAssembly.CompileError,
                   /GC feature version.*no longer supported by this engine/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in ${SOME_FUTURE_VERSION}))`)),
                   WebAssembly.CompileError,
                   /GC feature version is unknown/);

// Parameters of ref type are only available if we opt in.

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in ${CURRENT_VERSION})
      (type (func (param anyref))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (type (func (param anyref))))`)),
                   WebAssembly.CompileError,
                   /reference types not enabled/);

// Ditto returns

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in ${CURRENT_VERSION})
      (type (func (result anyref))))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (type (func (result anyref))))`)),
                   WebAssembly.CompileError,
                   /reference types not enabled/);

// Ditto locals

new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in ${CURRENT_VERSION})
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
      (gc_feature_opt_in ${CURRENT_VERSION})
      (global (mut anyref) (ref.null)))`));

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (global (mut anyref) (ref.null)))`)),
                   WebAssembly.CompileError,
                   /reference types not enabled/);

// Ref instructions are only available if we opt in.
//
// When testing these we need to avoid struct types or parameters, locals,
// returns, or globals of ref type, or guards on those will preempt the guards
// on the instructions.

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func ref.null))`)),
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

