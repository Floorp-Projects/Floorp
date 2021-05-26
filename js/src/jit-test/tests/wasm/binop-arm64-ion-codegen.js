// |jit-test| skip-if: !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration().arm64; include:codegen-arm64-test.js

// Test that multiplication by -1 yields negation.

let neg32 =
    `(module
       (func (export "f") (param i32) (result i32)
         (i32.mul (local.get 0) (i32.const -1))))`;
codegenTestARM64_adhoc(
    neg32,
    'f',
    '4b0003e0  neg w0, w0');
assertEq(wasmEvalText(neg32).exports.f(-37), 37)
assertEq(wasmEvalText(neg32).exports.f(42), -42)

let neg64 = `(module
       (func (export "f") (param i64) (result i64)
         (i64.mul (local.get 0) (i64.const -1))))`
codegenTestARM64_adhoc(
    neg64,
    'f',
    'cb0003e0  neg x0, x0');
assertEq(wasmEvalText(neg64).exports.f(-37000000000n), 37000000000n)
assertEq(wasmEvalText(neg64).exports.f(42000000000n), -42000000000n)

// Test that multiplication by zero yields zero

let zero32 =
    `(module
       (func (export "f") (param i32) (result i32)
         (i32.mul (local.get 0) (i32.const 0))))`;
codegenTestARM64_adhoc(
    zero32,
    'f',
    '2a1f03e0  mov w0, wzr');
assertEq(wasmEvalText(zero32).exports.f(-37), 0)
assertEq(wasmEvalText(zero32).exports.f(42), 0)

let zero64 = `(module
       (func (export "f") (param i64) (result i64)
         (i64.mul (local.get 0) (i64.const 0))))`
codegenTestARM64_adhoc(
    zero64,
    'f',
    'aa1f03e0  mov x0, xzr');
assertEq(wasmEvalText(zero64).exports.f(-37000000000n), 0n)
assertEq(wasmEvalText(zero64).exports.f(42000000000n), 0n)

// Test that multiplication by one yields no code

let one32 =
    `(module
       (func (export "f") (param i32) (result i32)
         (i32.mul (local.get 0) (i32.const 1))))`;
codegenTestARM64_adhoc(
    one32,
    'f',
    '');
assertEq(wasmEvalText(one32).exports.f(-37), -37)
assertEq(wasmEvalText(one32).exports.f(42), 42)

let one64 = `(module
       (func (export "f") (param i64) (result i64)
         (i64.mul (local.get 0) (i64.const 1))))`
codegenTestARM64_adhoc(
    one64,
    'f',
    '');
assertEq(wasmEvalText(one64).exports.f(-37000000000n), -37000000000n)
assertEq(wasmEvalText(one64).exports.f(42000000000n), 42000000000n)

// Test that multiplication by two yields an add

let double32 =
    `(module
       (func (export "f") (param i32) (result i32)
         (i32.mul (local.get 0) (i32.const 2))))`;
codegenTestARM64_adhoc(
    double32,
    'f',
    '2b000000  adds w0, w0, w0'); // The ADDS is some legacy thing, likely unnecessary
assertEq(wasmEvalText(double32).exports.f(-37), -74)
assertEq(wasmEvalText(double32).exports.f(42), 84)

let double64 = `(module
       (func (export "f") (param i64) (result i64)
         (i64.mul (local.get 0) (i64.const 2))))`
codegenTestARM64_adhoc(
    double64,
    'f',
    '8b000000  add x0, x0, x0');
assertEq(wasmEvalText(double64).exports.f(-37000000000n), -74000000000n)
assertEq(wasmEvalText(double64).exports.f(42000000000n), 84000000000n)

// Test that multiplication by four yields a shift

let quad32 =
    `(module
       (func (export "f") (param i32) (result i32)
         (i32.mul (local.get 0) (i32.const 4))))`;
codegenTestARM64_adhoc(
    quad32,
    'f',
    '531e7400  lsl w0, w0, #2');
assertEq(wasmEvalText(quad32).exports.f(-37), -148)
assertEq(wasmEvalText(quad32).exports.f(42), 168)

let quad64 = `(module
       (func (export "f") (param i64) (result i64)
         (i64.mul (local.get 0) (i64.const 4))))`
codegenTestARM64_adhoc(
    quad64,
    'f',
    'd37ef400  lsl x0, x0, #2');
assertEq(wasmEvalText(quad64).exports.f(-37000000000n), -148000000000n)
assertEq(wasmEvalText(quad64).exports.f(42000000000n), 168000000000n)

// Test that multiplication by five yields a multiply

let quint32 =
    `(module
       (func (export "f") (param i32) (result i32)
         (i32.mul (local.get 0) (i32.const 5))))`;
codegenTestARM64_adhoc(
    quint32,
    'f',
    `528000b0  mov     w16, #0x5
     1b107c00  mul     w0, w0, w16`);
assertEq(wasmEvalText(quint32).exports.f(-37), -37*5)
assertEq(wasmEvalText(quint32).exports.f(42), 42*5)

let quint64 = `(module
       (func (export "f") (param i64) (result i64)
         (i64.mul (local.get 0) (i64.const 5))))`
codegenTestARM64_adhoc(
    quint64,
    'f',
    `d28000b0  mov     x16, #0x5
     9b107c00  mul     x0, x0, x16`);
assertEq(wasmEvalText(quint64).exports.f(-37000000000n), -37000000000n*5n)
assertEq(wasmEvalText(quint64).exports.f(42000000000n), 42000000000n*5n)

// Test that add/sub/and/or/xor don't need to reuse their input register.  The
// proof here is that the destination register does not equal any of the input
// registers.
//
// We have adequate functionality tests for these elsewhere, so test only
// codegen here.

for ( [op, imm, expectVar, expectImm] of
      [['and', 64,
        '8a020020  and     x0, x1, x2',
        '927a0020  and     x0, x1, #0x40'],
       ['or', 64,
        'aa020020  orr     x0, x1, x2',
        'b27a0020  orr     x0, x1, #0x40'],
       ['xor', 64,
        'ca020020  eor     x0, x1, x2',
        'd27a0020  eor     x0, x1, #0x40'],
       ['add', 64,
        '8b020020  add     x0, x1, x2',
        '91010020  add     x0, x1, #0x40 \\(64\\)'],
       ['sub', 64,
        'cb020020  sub     x0, x1, x2',
        'd1010020  sub     x0, x1, #0x40 \\(64\\)']] ) {
    codegenTestARM64_adhoc(`
(module
  (func (export "f") (param i64) (param i64) (param i64) (result i64)
    (i64.${op} (local.get 1) (local.get 2))))`,
                           'f',
                           expectVar);
    codegenTestARM64_adhoc(`
(module
  (func (export "f") (param i64) (param i64) (result i64)
    (i64.${op} (local.get 1) (i64.const ${imm}))))`,
                           'f',
                           expectImm);
}

// Test that shifts and rotates with a constant don't need to reuse their input
// register.  The proof here is that the destination register does not equal any
// of the input registers.
//
// We have adequate functionality tests for these elsewhere, so test only
// codegen here.

for ( [op, expect] of
      [['shl',   'd37ef420  lsl     x0, x1, #2'],
       ['shr_s', '9342fc20  asr     x0, x1, #2'],
       ['shr_u', 'd342fc20  lsr     x0, x1, #2'],
       ['rotl',  '93c1f820  ror     x0, x1, #62'],
       ['rotr',  '93c10820  ror     x0, x1, #2']] ) {
    codegenTestARM64_adhoc(`
(module
  (func (export "f") (param i64) (param i64) (result i64)
    (i64.${op} (local.get 1) (i64.const 2))))`,
                           'f',
                           expect);
}
