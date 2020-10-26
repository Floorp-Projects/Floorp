// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves or fixups for various SIMD comparison
// operations.  See README-codegen.md for general information about this type of
// test case.

// Inputs (xmm0, xmm1)

for ( let [op, expected] of [
    ['i8x16.gt_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 64 c1               pcmpgtb %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.gt_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 65 c1               pcmpgtw %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.gt_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 66 c1               pcmpgtd %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.le_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 64 c1               pcmpgtb %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.le_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 65 c1               pcmpgtw %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.le_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 66 c1               pcmpgtd %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.eq',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 74 c1               pcmpeqb %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.eq',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 75 c1               pcmpeqw %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.eq',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 76 c1               pcmpeqd %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.ne',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 74 c1               pcmpeqb %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.ne',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 75 c1               pcmpeqw %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.ne',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 76 c1               pcmpeqd %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.eq',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f c2 c1 00               cmpps \\$0x00, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.ne',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f c2 c1 04               cmpps \\$0x04, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.lt',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f c2 c1 01               cmpps \\$0x01, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.le',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f c2 c1 02               cmpps \\$0x02, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.eq',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f c2 c1 00            cmppd \\$0x00, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.ne',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f c2 c1 04            cmppd \\$0x04, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.lt',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f c2 c1 01            cmppd \\$0x01, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.le',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f c2 c1 02            cmppd \\$0x02, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
] ) {
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (func (export "f") (param v128) (param v128) (result v128)
      (${op} (local.get 0) (local.get 1))))
        `)));
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}

// Inputs (xmm1, xmm0) because the operation reverses its arguments.

for ( let [op, expected] of [
    ['i8x16.ge_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 64 c1               pcmpgtb %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.ge_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 65 c1               pcmpgtw %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.ge_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 66 c1               pcmpgtd %xmm1, %xmm0
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x0000000000000040, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.lt_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 64 c1               pcmpgtb %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.lt_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 65 c1               pcmpgtw %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.lt_s',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 66 c1               pcmpgtd %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.gt',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f c2 c1 01               cmpps \\$0x01, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.ge',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f c2 c1 02               cmpps \\$0x02, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.gt',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f c2 c1 01            cmppd \\$0x01, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.ge',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f c2 c1 02            cmppd \\$0x02, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
] ) {
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (func (export "f") (param v128) (param v128) (result v128)
      (${op} (local.get 1) (local.get 0))))
        `)));
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}
