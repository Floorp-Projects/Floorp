// |jit-test| skip-if: !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator"); include:codegen-x64-test.js

// This file checks folding rules for wasm {and,or,xor}{32,64} on x64 via Ion.
// See also binop-x64-ion-codegen.js, which is similar.

// These tests check that a folding has happened.  The function is also run
// and the result value checked.  Note, many of the functions ignore one or
// both of their parameters.
//
// In many cases the expected code contains an unnecessary move via a
// temporary, generally e/rcx, for example:
//
//   mov %rdi, %rcx
//   mov %rcx, %rax
//
// where rdi is the first arg reg (on Linux) and rax is the retval reg.  This
// is documented in bug 1701164.  If/when that gets fixed, we will presumably
// have to redo the expected-outputs here.
//
// The situation is complicated further because many of the expected outputs
// here depend on one of the two argument registers, but they are different in
// the ELF ABI (rdi, rsi) and the Win64 ABI (rcx, rdx).  For the latter case,
// the abovementioned spurious move from the first arg reg into rcx
// disappears.  Hence there is some fudging using wildcards in register names,
// and `no_prefix`, to make the tests pass with both ABIs.

function test(ty, wasm_insn, must_appear, param0, param1, expected_result,
              options = {}) {
    let t =
        `(module
           (func (export "f") (param ${ty}) (param ${ty}) (result ${ty})
             ${wasm_insn}
        ))`;
    options.instanceBox = {value: null};
    codegenTestX64_adhoc(t, "f", must_appear, options);
    let ins = options.instanceBox.value;
    assertEq(ins.exports.f(param0, param1), expected_result);
}

function test32(wasm_insn, must_appear, param0, param1, expected_result,
                options) {
    return test('i32', wasm_insn, must_appear, param0, param1, expected_result,
                options);
}

function test64(wasm_insn, must_appear, param0, param1, expected_result,
                options) {
    return test('i64', wasm_insn, must_appear, param0, param1, expected_result,
               options);
}

// {AND,OR,XOR}{32,64} folding: both args const

test32('(i32.and (i32.const 0x12345678) (i32.const 0x0f0f0f0f))',
       'b8 08 06 04 02   mov \\$0x2040608, %eax',
       0,0, 0x2040608);
test64('(i64.and (i64.const 0x1234567851505150) (i64.const 0x515051500f0f0f0f))',
       '48 b8 00 01 00 01 50 50 10 10   mov \\$0x1010505001000100, %rax',
       0n,0n, 0x1010505001000100n);

test32('(i32.or (i32.const 0x12345678) (i32.const 0x0f0e0d0c))',
       'b8 7c 5f 3e 1f   mov \\$0x1F3E5F7C, %eax',
       0,0, 0x1f3e5f7c);
test64('(i64.or (i64.const 0x1234567851505150) (i64.const 0x515051500f0f1337))',
       '48 b8 77 53 5f 5f 78 57 74 53   mov \\$0x537457785F5F5377, %rax',
       0n,0n, 0x537457785f5f5377n);

test32('(i32.xor (i32.const 0x12345678) (i32.const 0x0f0e0d0c))',
       'b8 74 5b 3a 1d   mov \\$0x1D3A5B74, %eax',
       0,0, 0x1d3a5b74);
test64('(i64.xor (i64.const 0x1234567851505150) (i64.const 0x515051500f0f1337))',
       '48 b8 67 42 5f 5e 28 07 64 43   mov \\$0x436407285E5F4267, %rax',
       0n,0n, 0x436407285e5f4267n);

// {AND,OR,XOR}{32,64} identities: first arg is all zeroes

test32('(i32.and (i32.const 0) (local.get 1))',
       '33 c0   xor %eax, %eax',
       1234,5678, 0);
test64('(i64.and (i64.const 0) (local.get 1))',
       '33 c0   xor %eax, %eax',
       1234n,5678n, 0n);

test32('(i32.or (i32.const 0) (local.get 1))',
       `8b ..   mov %e.., %ecx
        8b c1   mov %ecx, %eax`,
       1234,5678, 5678);
test64('(i64.or (i64.const 0) (local.get 1))',
       `48 89 ..   mov %r.., %rcx
        48 89 c8   mov %rcx, %rax`,
       1234n,5678n, 5678n);

test32('(i32.xor (i32.const 0) (local.get 1))',
       `8b ..   mov %e.., %ecx
        8b c1   mov %ecx, %eax`,
       1234,5678, 5678);
test64('(i64.xor (i64.const 0) (local.get 1))',
       `48 89 ..   mov %r.., %rcx
        48 89 c8   mov %rcx, %rax`,
       1234n,5678n, 5678n);

// {AND,OR,XOR}{32,64} identities: second arg is all zeroes

test32('(i32.and (local.get 0) (i32.const 0))',
       '33 c0   xor %eax, %eax',
       1234,5678, 0);
test64('(i64.and (local.get 0) (i64.const 0))',
       '33 c0   xor %eax, %eax',
       1234n,5678n, 0n);

test32('(i32.or (local.get 0) (i32.const 0))',
     // 8b cf   mov %edi, %ecx  -- expected on Linux but not on Windows
       `8b c1   mov %ecx, %eax`,
       1234,5678, 1234, {no_prefix: true}); // required on Linux
test64('(i64.or (local.get 0) (i64.const 0))',
     // 48 89 f9   mov %rdi, %rcx  -- ditto
       `48 89 c8   mov %rcx, %rax`,
       1234n,5678n, 1234n, {no_prefix: true});

test32('(i32.xor (local.get 0) (i32.const 0))',
     // 8b cf   mov %edi, %ecx  -- ditto
       `8b c1   mov %ecx, %eax`,
       1234,5678, 1234, {no_prefix: true});
test64('(i64.xor (local.get 0) (i64.const 0))',
     // 48 89 f9   mov %rdi, %rcx  -- ditto
       `48 89 c8   mov %rcx, %rax`,
       1234n,5678n, 1234n, {no_prefix: true});

// {AND,OR,XOR}{32,64} identities: first arg is all ones

test32('(i32.and (i32.const 0xffffffff) (local.get 1))',
       `8b ..   mov %e.., %ecx
        8b c1   mov %ecx, %eax`,
       1234,5678, 5678);
test64('(i64.and (i64.const 0xffffffffffffffff) (local.get 1))',
       `48 89 ..   mov %r.., %rcx
        48 89 c8   mov %rcx, %rax`,
       1234n,5678n, 5678n);

test32('(i32.or (i32.const 0xffffffff) (local.get 1))',
       'b8 ff ff ff ff   mov \\$-0x01, %eax',
       1234,5678, -1/*0xffffffff*/);
test64('(i64.or (i64.const 0xffffffffffffffff) (local.get 1))',
       '48 c7 c0 ff ff ff ff   mov \\$-0x01, %rax',
       1234n,5678n, -1n/*0xffffffffffffffff*/);

test32('(i32.xor (i32.const 0xffffffff) (local.get 1))',
       `8b ..   mov %e.., %ecx
        8b c1   mov %ecx, %eax
        f7 d0   not %eax`,
       1234,5678, -5679);
test64('(i64.xor (i64.const 0xffffffffffffffff) (local.get 1))',
       `48 89 ..   mov %r.., %rcx
        48 89 c8   mov %rcx, %rax
        48 f7 d0   not %rax`,
       1234n,5678n, -5679n);

// {AND,OR,XOR}{32,64} identities: second arg is all ones

test32('(i32.and (local.get 0) (i32.const 0xffffffff))',
     // 8b cf   mov %edi, %ecx  -- expected on Linux but not on Windows
       `8b c1   mov %ecx, %eax`,
       1234,5678, 1234, {no_prefix: true}); // required on Linux
test64('(i64.and (local.get 0) (i64.const 0xffffffffffffffff))',
     // 48 89 f9   mov %rdi, %rcx  -- ditto
       `48 89 c8   mov %rcx, %rax`,
       1234n,5678n, 1234n, {no_prefix: true});

test32('(i32.or (local.get 0) (i32.const 0xffffffff))',
       'b8 ff ff ff ff   mov \\$-0x01, %eax',
       1234,5678, -1/*0xffffffff*/);
test64('(i64.or (local.get 0) (i64.const 0xffffffffffffffff))',
       '48 c7 c0 ff ff ff ff   mov \\$-0x01, %rax',
       1234n,5678n, -1n/*0xffffffffffffffff*/);

test32('(i32.xor (local.get 0) (i32.const 0xffffffff))',
     // 8b cf   mov %edi, %ecx  -- ditto
       `8b c1   mov %ecx, %eax
        f7 d0   not %eax`,
       1234,5678, -1235, {no_prefix: true});
test64('(i64.xor (local.get 0) (i64.const 0xffffffffffffffff))',
     // 48 89 f9   mov %rdi, %rcx  -- ditto
       `48 89 c8   mov %rcx, %rax
        48 f7 d0   not %rax`,
       1234n,5678n, -1235n, {no_prefix: true});

// {AND,OR,XOR}{32,64} identities: both args the same

test32('(i32.and (local.get 0) (local.get 0))',
     // 8b cf   mov %edi, %ecx  -- ditto
       `8b c1   mov %ecx, %eax`,
       1234,5678, 1234, {no_prefix: true});
test64('(i64.and (local.get 0) (local.get 0))',
     // 48 89 f9   mov %rdi, %rcx  -- ditto
       `48 89 c8   mov %rcx, %rax`,
       1234n,5678n, 1234n, {no_prefix: true});

test32('(i32.or (local.get 0) (local.get 0))',
     // 8b cf   mov %edi, %ecx  -- ditto
       `8b c1   mov %ecx, %eax`,
       1234,5678, 1234, {no_prefix: true});
test64('(i64.or (local.get 0) (local.get 0))',
     // 48 89 f9   mov %rdi, %rcx  -- ditto
       `48 89 c8   mov %rcx, %rax`,
       1234n,5678n, 1234n, {no_prefix: true});

test32('(i32.xor (local.get 0) (local.get 0))',
       '33 c0   xor %eax, %eax',
       1234,5678, 0);
test64('(i64.xor (local.get 0) (local.get 0))',
       '33 c0   xor %eax, %eax',
       1234n,5678n, 0n);
