// |jit-test| skip-if: !hasDisassembler() || wasmCompileMode() != "ion" || getBuildConfiguration("windows") || (!getBuildConfiguration("x64") && !getBuildConfiguration("x86") && !getBuildConfiguration("arm64") && !getBuildConfiguration("arm")); include:adhoc-multiplatform-test.js
//
// These tests push wasm functions through the ion pipe and specify an expected
// disassembly output on all 4 primary targets, x64 / x86 / arm64 / arm(32).
// Results must be provided for the first 3, but can optionally be skipped
// for arm(32).
//
// Hence: disassembler is needed, compiler must be ion.
//
// Windows is disallowed because the argument registers are different from on
// Linux, and matching both is both difficult and not of much value.

// Tests are "end-to-end" in the sense that we don't care whether the
// tested-for code improvement is done by MIR optimisation, or later in the
// pipe.  Observed defects are marked with FIXMEs for future easy finding.


// Note that identities involving AND, OR and XOR are tested by
// binop-x64-ion-folding.js

// Multiplication with magic constant on the left
//
//    0 * x =>  0
//    1 * x =>  x
//   -1 * x =>  -x
//    2 * x =>  x + x
//    4 * x =>  x << 2

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_zeroL") (param $p1 i32) (result i32)
       (i32.mul (i32.const 0) (local.get $p1))))`,
    "mul32_zeroL",
    {x64:   // FIXME move folding to MIR level
            // First we move edi to eax unnecessarily via ecx (bug 1752520),
            // then we overwrite eax.  Presumably because the folding
            // 0 * x => 0 is done at the LIR level, not the MIR level, hence
            // the now-pointless WasmParameter node is not DCE'd away, since
            // DCE only happens at the MIR level.  In fact all targets suffer
            // from the latter problem, but on x86 no_prefix_x86:true
            // hides it, and on arm32/64 the pointless move is correctly
            // transformed by RA into a no-op.
            `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax
             33 c0     xor %eax, %eax`,
     x86:   `33 c0     xor %eax, %eax`,
     arm64: `2a1f03e0  mov w0, wzr`,
     arm:   `e3a00000  mov r0, #0`},
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_zeroL") (param $p1 i64) (result i64)
       (i64.mul (i64.const 0) (local.get $p1))))`,
    "mul64_zeroL",
    // FIXME folding happened, zero-creation insns could be improved
    {x64:   // Same shenanigans as above.  Also, on xor, REX.W is redundant.
            `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax
             48 33 c0  xor %rax, %rax`,
     x86:   `33 c0     xor %eax, %eax
             33 d2     xor %edx, %edx`,
     arm64: `aa1f03e0  mov x0, xzr`,
     arm:   // bizarrely inconsistent with the 32-bit case
            `e0200000  eor r0, r0, r0
             e0211001  eor r1, r1, r1` },
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_oneL") (param $p1 i32) (result i32)
       (i32.mul (i32.const 1) (local.get $p1))))`,
    "mul32_oneL",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``},
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_oneL") (param $p1 i64) (result i64)
       (i64.mul (i64.const 1) (local.get $p1))))`,
    "mul64_oneL",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``},
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_minusOneL") (param $p1 i32) (result i32)
       (i32.mul (i32.const -1) (local.get $p1))))`,
    "mul32_minusOneL",
    {x64:   `f7 d8     neg %eax`,
     x86:   `f7 d8     neg %eax`,
     arm64: `4b0003e0  neg w0, w0`,
     arm:   `e2600000  rsb r0, r0, #0`},
    {x86: {no_prefix:true}, x64: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_minusOneL") (param $p1 i64) (result i64)
       (i64.mul (i64.const -1) (local.get $p1))))`,
    "mul64_minusOneL",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax
             48 f7 d8  neg %rax`,
     x86:   `f7 d8     neg %eax
             83 d2 00  adc \\$0x00, %edx
             f7 da     neg %edx`,
     arm64: `cb0003e0  neg  x0, x0`,
     arm:   `e2700000  rsbs r0, r0, #0
             e2e11000  rsc  r1, r1, #0`},
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_twoL") (param $p1 i32) (result i32)
       (i32.mul (i32.const 2) (local.get $p1))))`,
    "mul32_twoL",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax
             03 c0     add %eax, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax
             03 c0     add %eax, %eax`,
     arm64: `0b000000  add w0, w0, w0`,
     arm:   `e0900000  adds r0, r0, r0`},
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_twoL") (param $p1 i64) (result i64)
       (i64.mul (i64.const 2) (local.get $p1))))`,
    "mul64_twoL",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax
             48 03 c0  add %rax, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax
             03 c0     add %eax, %eax
             13 d2     adc %edx, %edx`,
     arm64: `8b000000  add  x0, x0, x0`,
     arm:   `e0900000  adds r0, r0, r0
             e0a11001  adc  r1, r1, r1`},
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_fourL") (param $p1 i32) (result i32)
       (i32.mul (i32.const 4) (local.get $p1))))`,
    "mul32_fourL",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax
             c1 e0 02  shl \\$0x02, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax
             c1 e0 02  shl \\$0x02, %eax`,
     arm64: `531e7400  lsl w0, w0, #2`,
     arm:   `e1a00100  mov r0, r0, lsl #2`},
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_fourL") (param $p1 i64) (result i64)
       (i64.mul (i64.const 4) (local.get $p1))))`,
    "mul64_fourL",
    {x64:   `48 89 f9     mov %rdi, %rcx
             48 89 c8     mov %rcx, %rax
             48 c1 e0 02  shl \\$0x02, %rax`,
     x86:   `8b 55 14     movl 0x14\\(%rbp\\), %edx
             8b 45 10     movl 0x10\\(%rbp\\), %eax
             0f a4 c2 02  shld \\$0x02, %eax, %edx
             c1 e0 02     shl \\$0x02, %eax`,
     arm64: `d37ef400     lsl x0, x0, #2`,
     arm:   `e1a01101     mov r1, r1, lsl #2
             e1811f20     orr r1, r1, r0, lsr #30
             e1a00100     mov r0, r0, lsl #2`},
    {x86: {no_prefix:true}}
);

// Multiplication with magic constant on the right
//
//  x * 0  =>  0
//  x * 1  =>  x
//  x * -1 =>  -x
//  x * 2  =>  x + x
//  x * 4  =>  x << 2

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_zeroR") (param $p1 i32) (result i32)
       (i32.mul (local.get $p1) (i32.const 0))))`,
    "mul32_zeroR",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax
             33 c0     xor %eax, %eax`,
     x86:   `33 c0     xor %eax, %eax`,
     arm64: `2a1f03e0  mov w0, wzr`,
     arm:   `e3a00000  mov r0, #0`},
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_zeroR") (param $p1 i64) (result i64)
       (i64.mul (local.get $p1) (i64.const 0))))`,
    "mul64_zeroR",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax
             48 33 c0  xor %rax, %rax`,     // REX.W is redundant
     x86:   `33 c0     xor %eax, %eax
             33 d2     xor %edx, %edx`,
     arm64: `aa1f03e0  mov x0, xzr`,
     arm:   `e0200000  eor r0, r0, r0
             e0211001  eor r1, r1, r1` },
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_oneR") (param $p1 i32) (result i32)
       (i32.mul (local.get $p1) (i32.const 1))))`,
    "mul32_oneR",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``},
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_oneR") (param $p1 i64) (result i64)
       (i64.mul (local.get $p1) (i64.const 1))))`,
    "mul64_oneR",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``},
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_minusOneR") (param $p1 i32) (result i32)
       (i32.mul (local.get $p1) (i32.const -1))))`,
    "mul32_minusOneR",
    {x64:   `f7 d8     neg %eax`,
     x86:   `f7 d8     neg %eax`,
     arm64: `4b0003e0  neg w0, w0`,
     arm:   `e2600000  rsb r0, r0, #0`},
    {x86: {no_prefix:true}, x64: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_minusOneR") (param $p1 i64) (result i64)
       (i64.mul (local.get $p1) (i64.const -1))))`,
    "mul64_minusOneR",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax
             48 f7 d8  neg %rax`,
     x86:   `f7 d8     neg %eax
             83 d2 00  adc \\$0x00, %edx
             f7 da     neg %edx`,
     arm64: `cb0003e0  neg  x0, x0`,
     arm:   `e2700000  rsbs r0, r0, #0
             e2e11000  rsc  r1, r1, #0`},
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_twoR") (param $p1 i32) (result i32)
       (i32.mul (local.get $p1) (i32.const 2))))`,
    "mul32_twoR",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax
             03 c0     add %eax, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax
             03 c0     add %eax, %eax`,
     arm64: `0b000000  add w0, w0, w0`,
     arm:   `e0900000  adds r0, r0, r0`},
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_twoR") (param $p1 i64) (result i64)
       (i64.mul (local.get $p1) (i64.const 2))))`,
    "mul64_twoR",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax
             48 03 c0  add %rax, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax
             03 c0     add %eax, %eax
             13 d2     adc %edx, %edx`,
     arm64: `8b000000  add  x0, x0, x0`,
     arm:   `e0900000  adds r0, r0, r0
             e0a11001  adc  r1, r1, r1`},
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "mul32_fourR") (param $p1 i32) (result i32)
       (i32.mul (local.get $p1) (i32.const 4))))`,
    "mul32_fourR",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax
             c1 e0 02  shl \\$0x02, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax
             c1 e0 02  shl \\$0x02, %eax`,
     arm64: `531e7400  lsl w0, w0, #2`,
     arm:   `e1a00100  mov r0, r0, lsl #2`},
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "mul64_fourR") (param $p1 i64) (result i64)
       (i64.mul (local.get $p1) (i64.const 4))))`,
    "mul64_fourR",
    {x64:   `48 89 f9     mov %rdi, %rcx
             48 89 c8     mov %rcx, %rax
             48 c1 e0 02  shl \\$0x02, %rax`,
     x86:   `8b 55 14     movl 0x14\\(%rbp\\), %edx
             8b 45 10     movl 0x10\\(%rbp\\), %eax
             0f a4 c2 02  shld \\$0x02, %eax, %edx
             c1 e0 02     shl \\$0x02, %eax`,
     arm64: `d37ef400     lsl x0, x0, #2`,
     arm:   `e1a01101     mov r1, r1, lsl #2
             e1811f20     orr r1, r1, r0, lsr #30
             e1a00100     mov r0, r0, lsl #2`
    },
    {x86: {no_prefix:true}}
);

// Shifts by zero (the right arg is zero)
//
// x >> 0  =>  x  (any shift kind: shl, shrU, shrS)

codegenTestMultiplatform_adhoc(
    `(module (func (export "shl32_zeroR") (param $p1 i32) (result i32)
       (i32.shl (local.get $p1) (i32.const 0))))`,
    "shl32_zeroR",
    // FIXME check these are consistently folded out at the MIR level
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: // Regalloc badness, plus not folded out at the MIR level
            `2a0003e2  mov w2, w0
             2a0203e1  mov w1, w2
             53007c20  lsr w0, w1, #0`, // Uhh.  lsr ?!
     arm:   `e1a02000  mov r2, r0
             e1a01002  mov r1, r2
             e1a00001  mov r0, r1`
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "shl64_zeroR") (param $p1 i64) (result i64)
       (i64.shl (local.get $p1) (i64.const 0))))`,
    "shl64_zeroR",
    // FIXME why is this code so much better than the 32-bit case?
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``, // no-op
     arm:   ``  // no-op
    },
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "shrU32_zeroR") (param $p1 i32) (result i32)
       (i32.shr_u (local.get $p1) (i32.const 0))))`,
    "shrU32_zeroR",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: `2a0003e2  mov w2, w0
             2a0203e1  mov w1, w2
             2a0103e0  mov w0, w1`,
     arm:   `e1a02000  mov r2, r0
             e1a01002  mov r1, r2
             e1a00001  mov r0, r1`
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "shrU64_zeroR") (param $p1 i64) (result i64)
       (i64.shr_u (local.get $p1) (i64.const 0))))`,
    "shrU64_zeroR",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``
    },
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "shrS32_zeroR") (param $p1 i32) (result i32)
       (i32.shr_s (local.get $p1) (i32.const 0))))`,
    "shrS32_zeroR",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: `2a0003e2  mov w2, w0
             2a0203e1  mov w1, w2
             13007c20  sbfx w0, w1, #0, #32`,
     arm:   `e1a02000  mov r2, r0
             e1a01002  mov r1, r2
             e1a00001  mov r0, r1`
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "shrS64_zeroR") (param $p1 i64) (result i64)
       (i64.shr_s (local.get $p1) (i64.const 0))))`,
    "shrS64_zeroR",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``
    },
    {x86: {no_prefix:true}}
);

// Identities involving addition
//
//  x + 0   =>  x
//  0 + x   =>  x
//  x + x   =>  x << 1

codegenTestMultiplatform_adhoc(
    `(module (func (export "add32_zeroR") (param $p1 i32) (result i32)
       (i32.add (local.get $p1) (i32.const 0))))`,
    "add32_zeroR",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "add64_zeroR") (param $p1 i64) (result i64)
       (i64.add (local.get $p1) (i64.const 0))))`,
    "add64_zeroR",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``
    },
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "add32_zeroL") (param $p1 i32) (result i32)
       (i32.add (i32.const 0) (local.get $p1))))`,
    "add32_zeroL",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "add64_zeroL") (param $p1 i64) (result i64)
       (i64.add (i64.const 0) (local.get $p1))))`,
    "add64_zeroL",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``
    },
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "add32_self") (param $p1 i32) (result i32)
       (i32.add (local.get $p1) (local.get $p1))))`,
    "add32_self",
    {x64:   `8b cf  mov  %edi, %ecx
             8b c1  mov  %ecx, %eax
             03 c1  add  %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax
             03 45 10  addl 0x10\\(%rbp\\), %eax`,
     arm64: `0b000000  add  w0, w0, w0`,
     arm:   `e0900000  adds r0, r0, r0 `
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "add64_self") (param $p1 i64) (result i64)
       (i64.add (local.get $p1) (local.get $p1))))`,
    "add64_self",
    // FIXME outstandingly bad 32-bit sequences, probably due to the RA
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax
             48 03 c1  add %rcx, %rax`,
     x86:   // -0x21524111 is 0xDEADBEEF
            `8b 5d 14        movl 0x14\\(%rbp\\), %ebx
             8b 4d 10        movl 0x10\\(%rbp\\), %ecx
             bf ef be ad de  mov \\$-0x21524111, %edi
             8b 55 14        movl 0x14\\(%rbp\\), %edx
             8b 45 10        movl 0x10\\(%rbp\\), %eax
             03 c1           add %ecx, %eax
             13 d3           adc %ebx, %edx`,
     arm64: `8b000000  add  x0, x0, x0`,
     arm:   // play Musical Chairs for a while
            `e1a03001  mov  r3, r1
             e1a02000  mov  r2, r0
             e1a05003  mov  r5, r3
             e1a04002  mov  r4, r2
             e1a01003  mov  r1, r3
             e1a00002  mov  r0, r2
             e0900004  adds r0, r0, r4
             e0a11005  adc  r1, r1, r5`
    },
    {x86: {no_prefix:true}}
);

// Identities involving subtraction
//
//  x - 0   =>  x
//  0 - x   =>  -x
//  x - x   =>  0

codegenTestMultiplatform_adhoc(
    `(module (func (export "sub32_zeroR") (param $p1 i32) (result i32)
       (i32.sub (local.get $p1) (i32.const 0))))`,
    "sub32_zeroR",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax`,
     arm64: ``,
     arm:   ``
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "sub64_zeroR") (param $p1 i64) (result i64)
       (i64.sub (local.get $p1) (i64.const 0))))`,
    "sub64_zeroR",
    // FIXME folding missing for all 4 targets
    {x64:   `48 89 f9     mov %rdi, %rcx
             48 89 c8     mov %rcx, %rax
             48 83 e8 00  sub \\$0x00, %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax
             83 ea 00  sub  \\$0x00, %edx`,
     arm64: ``,
     arm:   `e2500000  subs r0, r0, #0
             e2c11000  sbc  r1, r1, #0`
    },
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "sub32_zeroL") (param $p1 i32) (result i32)
       (i32.sub (i32.const 0) (local.get $p1))))`,
    "sub32_zeroL",
    {x64:   `8b cf     mov %edi, %ecx
             8b c1     mov %ecx, %eax
             f7 d8     neg %eax`,
     x86:   `8b 45 10  movl 0x10\\(%rbp\\), %eax
             f7 d8     neg %eax`,
     arm64: `4b0003e0  neg w0, w0 `,
     arm:   `e2600000  rsb r0, r0, #0`
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "sub64_zeroL") (param $p1 i64) (result i64)
       (i64.sub (i64.const 0) (local.get $p1))))`,
    "sub64_zeroL",
    {x64:   `48 89 f9  mov %rdi, %rcx
             48 89 c8  mov %rcx, %rax
             48 f7 d8  neg %rax`,
     x86:   `8b 55 14  movl 0x14\\(%rbp\\), %edx
             8b 45 10  movl 0x10\\(%rbp\\), %eax
             f7 d8     neg %eax
             83 d2 00  adc \\$0x00, %edx
             f7 da     neg %edx`,
     arm64: `cb0003e0  neg  x0, x0`,
     arm:   `e2700000  rsbs r0, r0, #0
             e2e11000  rsc  r1, r1, #0`
    },
    {x86: {no_prefix:true}}
);

codegenTestMultiplatform_adhoc(
    `(module (func (export "sub32_self") (param $p1 i32) (result i32)
       (i32.sub (local.get $p1) (local.get $p1))))`,
    "sub32_self",
    {x64:   `33 c0  xor %eax, %eax`,
     x86:   `33 c0  xor %eax, %eax`,
     arm64: `52800000  mov w0, #0x0`,
     arm:   `e3a00000  mov r0, #0`
    },
    {x86: {no_prefix:true}}
);
codegenTestMultiplatform_adhoc(
    `(module (func (export "sub64_self") (param $p1 i64) (result i64)
       (i64.sub (local.get $p1) (local.get $p1))))`,
    "sub64_self",
    // FIXME folding missing for all 4 targets
    {x64:   `48 89 f9        mov %rdi, %rcx
             48 89 c8        mov %rcx, %rax
             48 2b c1        sub %rcx, %rax`,
     x86:   // -0x21524111 is 0xDEADBEEF
            `8b 5d 14        movl 0x14\\(%rbp\\), %ebx
             8b 4d 10        movl 0x10\\(%rbp\\), %ecx
             bf ef be ad de  mov  \\$-0x21524111, %edi
             8b 55 14        movl 0x14\\(%rbp\\), %edx
             8b 45 10        movl 0x10\\(%rbp\\), %eax
             2b c1           sub %ecx, %eax
             1b d3           sbb %ebx, %edx`,
     arm64: `cb000000  sub  x0, x0, x0`,
     arm:   `e1a03001  mov  r3, r1
             e1a02000  mov  r2, r0
             e1a05003  mov  r5, r3
             e1a04002  mov  r4, r2
             e1a01003  mov  r1, r3
             e1a00002  mov  r0, r2
             e0500004  subs r0, r0, r4
             e0c11005  sbc  r1, r1, r5`
    },
    {x86: {no_prefix:true}}
);
