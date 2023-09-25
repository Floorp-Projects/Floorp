// |jit-test| skip-if: !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("arm64"); include:codegen-arm64-test.js

// Test that loads/stores at friendly constant offsets yield expected code

codegenTestARM64_adhoc(
    `(module
       (memory 1)
       (func (export "f") (result i32)
         (i32.load (i32.const 4000))))`,
    'f',
    'b94fa2a0  ldr  w0, \\[x21, #4000\\]');

codegenTestARM64_adhoc(
    `(module
       (memory 1)
       (func (export "f") (result i32)
         (i32.load offset=1000 (i32.const 3000))))`,
    'f',
    'b94fa2a0  ldr  w0, \\[x21, #4000\\]');

codegenTestARM64_adhoc(
    `(module
       (memory 1)
       (func (export "f") (param i32)
         (i32.store (i32.const 4000) (local.get 0))))`,
    'f',
    'b90fa2a0  str w0, \\[x21, #4000\\]');

codegenTestARM64_adhoc(
    `(module
       (memory 1)
       (func (export "f") (param i32)
         (i32.store offset=1000 (i32.const 3000) (local.get 0))))`,
    'f',
    'b90fa2a0  str w0, \\[x21, #4000\\]');

// Unfriendly offsets are first loaded into a scratch register

codegenTestARM64_adhoc(
    `(module
       (memory 1)
       (func (export "f") (result i32)
         (i32.load (i32.const 4001))))`,
    'f',
    `d281f430  mov     x16, #0xfa1
     b8706aa0  ldr     w0, \\[x21, x16\\]`);

codegenTestARM64_adhoc(
    `(module
       (memory 1)
       (func (export "f") (param i32)
         (i32.store (i32.const 4001) (local.get 0))))`,
    'f',
    `d281f430  mov     x16, #0xfa1
     b8306aa0  str     w0, \\[x21, x16\\]`);

