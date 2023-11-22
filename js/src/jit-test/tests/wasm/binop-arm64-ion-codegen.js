// |jit-test| skip-if: !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("arm64"); include:codegen-arm64-test.js

// Basic constant folding tests

for ( [op, lhs, rhs, expect] of
      [['add',   5,    8,    'd28001a0  mov     x0, #0xd'],
       ['sub',   4,    5,    '92800000  mov     x0, #0xffffffffffffffff'],
       ['mul',   8,    3,    'd2800300  mov     x0, #0x18'],
       ['div_s', -8,   3,    '92800020  mov     x0, #0xfffffffffffffffe'],
       ['div_u', 8,    3,    'd2800040  mov     x0, #0x2'],
       ['rem_s', 8,    5,    'd2800060  mov     x0, #0x3'],
       ['rem_u', -7,   4,    'd2800020  mov     x0, #0x1'],
       ['and',   0xfe, 0x77, 'd2800ec0  mov     x0, #0x76'],
       ['or',    0xfe, 0x77, 'd2801fe0  mov     x0, #0xff'],
       ['xor',   0xfe, 0x77, 'd2801120  mov     x0, #0x89'],
       ['shl',   3,    4,    'd2800600  mov     x0, #0x30'],
       ['shr_s', -8,   1,    '92800060  mov     x0, #0xfffffffffffffffc'],
       ['shr_u', -8,   1,    'b27ef3e0  mov     x0, #0x7ffffffffffffffc']] ) {
    codegenTestARM64_adhoc(`
(module
  (func (export "f") (result i64)
    (i64.${op} (i64.const ${lhs}) (i64.const ${rhs}))))`,
                           'f',
                           expect);
}

// Basic tests that addition and multiplication identities are collapsed, use
// arg 1 here to force an explicit move to be emitted.

for ( [op, args, expect] of
      [['add',   '(local.get 1) (i64.const 0)', 'aa0103e0  mov     x0, x1'],
       ['add',   '(i64.const 0) (local.get 1)', 'aa0103e0  mov     x0, x1'],
       ['mul',   '(local.get 1) (i64.const 1)', 'aa0103e0  mov     x0, x1'],
       ['mul',   '(i64.const 1) (local.get 1)', 'aa0103e0  mov     x0, x1']] ) {
    codegenTestARM64_adhoc(`
(module
  (func (export "f") (param i64) (param i64) (result i64)
    (i64.${op} ${args})))`,
                           'f',
                           expect);
}

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

// Test that multiplication by one yields no code (this optimization currently
// exists both in constant folding and in lowering).

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
    '0b000000  add w0, w0, w0');
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

// Test that 0-n yields negation.

let subneg32 =
    `(module
       (func (export "f") (param i32) (result i32)
         (i32.sub (i32.const 0) (local.get 0))))`
codegenTestARM64_adhoc(
    subneg32,
    'f',
    '4b0003e0  neg w0, w0');
assertEq(wasmEvalText(subneg32).exports.f(-37), 37)
assertEq(wasmEvalText(subneg32).exports.f(42), -42)

let subneg64 = `(module
       (func (export "f") (param i64) (result i64)
         (i64.sub (i64.const 0) (local.get 0))))`
codegenTestARM64_adhoc(
    subneg64,
    'f',
    'cb0003e0  neg x0, x0');
assertEq(wasmEvalText(subneg64).exports.f(-37000000000n), 37000000000n)
assertEq(wasmEvalText(subneg64).exports.f(42000000000n), -42000000000n)

// Test that select does something reasonable and does not tie its output to one
// of its inputs.

codegenTestARM64_adhoc(
    `(module
       (func (export "f") (param i64) (param i64) (param i64) (param i32) (result i64)
         (select (local.get 1) (local.get 2) (local.get 3))))`,
    'f',
    `6a03007f  tst     w3, w3
     9a821020  csel    x0, x1, x2, ne`)

codegenTestARM64_adhoc(
    `(module
       (func (export "f") (param f64) (param f64) (param f64) (param i32) (result f64)
         (select (local.get 1) (local.get 2) (local.get 3))))`,
    'f',
    `6a00001f  tst     w0, w0
     1e621c20  fcsel   d0, d1, d2, ne`)

// Here we test that no boolean is generated and then re-tested, and that
// operands are swapped so that we can use an immediate constant, and that the
// input is not tied to the output.

codegenTestARM64_adhoc(
    `(module
       (func (export "f") (param $a i32) (param $b i32) (param $c i32) (param $d i32) (result i32)
         (select (local.get $b) (local.get $d) (i32.lt_s (i32.const 0) (local.get $c)))))`,
    'f',
    `7100005f  cmp     w2, #0x0 \\(0\\)
     1a83c020  csel    w0, w1, w3, gt`)

codegenTestARM64_adhoc(
    `(module
       (func (export "f") (param $a f64) (param $b f64) (param $c f64) (param $d f64) (result f64)
         (select (local.get $b) (local.get $d) (f64.lt (f64.const 0) (local.get $c)))))`,
    'f',
    `2f00e400  movi    d0, #0x0
     1e622000  fcmp    d0, d2
     1e633c20  fcsel   d0, d1, d3, lo`)

// FP ABS should not tie its input to its output.

codegenTestARM64_adhoc(
    `(module
       (func (export "f") (param f32) (param f32) (result f32)
         (f32.abs (local.get 1))))`,
    'f',
    '1e20c020  fabs    s0, s1');

codegenTestARM64_adhoc(
    `(module
       (func (export "f") (param f64) (param f64) (result f64)
         (f64.abs (local.get 1))))`,
    'f',
    '1e60c020  fabs    d0, d1');

// AND{32,64} followed by `== 0`: check the two operations are merged into a
// single 'tst' insn, and no 'and' insn.  The merging isn't done for
// {OR,XOR}{32,64}.  This is for both arguments being non-constant.

for ( [ty, expect_tst] of
      [['i32',   '6a01001f  tst w0, w1'],
       ['i64',   'ea01001f  tst x0, x1']] ) {
   codegenTestARM64_adhoc(
    `(module
       (func (export "f") (param $p1 ${ty}) (param $p2 ${ty}) (result i32)
         (local $x i32)
         (local.set $x (i32.const 0x4D2))
         (if (${ty}.eq (${ty}.and (local.get $p1) (local.get $p2))
                       (${ty}.const 0))
           (then (local.set $x (i32.const 0x11D7)))
         )
         (local.get $x)
       )
    )`,
    'f',
    `${expect_tst}
     54000061  b\\.ne  #\\+0xc \\(addr .*\\)
     52823ae0  mov     w0, #0x11d7
     14000002  b       #\\+0x8 \\(addr .*\\)
     52809a40  mov     w0, #0x4d2`
   );
}

// AND64 followed by `== 0`, with one of the args being a constant.

for ( [imm, expect1, expect2] of
      [ // as a valid logical-immediate => imm in insn
        ['0x0F0F0F0F0F0F0F0F',
         'f200cc1f  tst  x0, #0xf0f0f0f0f0f0f0f',
         ''],
        // anything else => imm synth'd into a reg
        ['-0x4771',
         '9288ee10  mov  x16, #0xffffffffffffb88f',
         'ea10001f  tst  x0, x16']]
      ) {
   codegenTestARM64_adhoc(
    `(module
       (func (export "f") (param $p1 i64) (result i32)
         (local $x i32)
         (local.set $x (i32.const 0x4D2))
         (if (i64.eq (i64.and (i64.const ${imm}) (local.get $p1))
                     (i64.const 0))
           (then (local.set $x (i32.const 0x11D7)))
         )
         (local.get $x)
       )
    )`,
    'f',
    `${expect1}
     ${expect2}
     54000061  b\\.ne  #\\+0xc \\(addr .*\\)
     52823ae0  mov     w0, #0x11d7
     14000002  b       #\\+0x8 \\(addr .*\\)
     52809a40  mov     w0, #0x4d2`
   );
}

// For integer comparison followed by select, check that the comparison result
// isn't materialised into a register, for specific types.

for ( [cmpTy, cmpOp, selTy, cmpRegPfx, cselRegPfx, armCC] of
      [ ['i32', 'le_s', 'i32',  'w', 'w', 'le'],
        ['i32', 'lt_u', 'i64',  'w', 'x', 'lo'],
        ['i64', 'le_s', 'i32',  'x', 'w', 'le'],
        ['i64', 'lt_u', 'i64',  'x', 'x', 'lo'],
      ] ) {
   codegenTestARM64_adhoc(
    `(module
       (func (export "f")
             (param $p1 ${cmpTy}) (param $p2 ${cmpTy})
             (param $p3 ${selTy}) (param $p4 ${selTy})
             (result ${selTy})
         (select (local.get $p3)
                 (local.get $p4)
                 (${cmpTy}.${cmpOp} (local.get $p1) (local.get $p2)))
       )
    )`,
    'f',
    `.b01001f  cmp  ${cmpRegPfx}0, ${cmpRegPfx}1
     .a83.040  csel ${cselRegPfx}0, ${cselRegPfx}2, ${cselRegPfx}3, ${armCC}`
   );
}
