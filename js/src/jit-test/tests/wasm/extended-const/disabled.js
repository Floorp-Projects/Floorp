// |jit-test| skip-if: wasmExtendedConstEnabled()

const { CompileError, validate } = WebAssembly;

const DISABLED = /extended constant expressions not enabled|unexpected initializer opcode/;

let tests = [
    "(module (global i32 i32.const 0 i32.const 0 i32.add))",
    "(module (global i32 i32.const 0 i32.const 0 i32.sub))",
    "(module (global i32 i32.const 0 i32.const 0 i32.mul))",
    "(module (global i64 i64.const 0 i64.const 0 i64.add))",
    "(module (global i64 i64.const 0 i64.const 0 i64.sub))",
    "(module (global i64 i64.const 0 i64.const 0 i64.mul))",
];

// Test that use of extended constants fails when disabled.

for (let src of tests) {
    let bin = wasmTextToBinary(src);
    assertEq(validate(bin), false);
    wasmCompilationShouldFail(bin, DISABLED);
}
