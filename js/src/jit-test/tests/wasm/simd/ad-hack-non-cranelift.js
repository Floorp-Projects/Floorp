// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode().indexOf("cranelift") >= 0; include:../tests/wasm/simd/ad-hack-preamble.js

// New instructions, not yet supported by cranelift

// Widening multiplication.
// This is to be moved into ad-hack.js
//
//   (iMxN.extmul_{high,low}_iKxL_{s,u} A B)
//
// is equivalent to
//
//   (iMxN.mul (iMxN.extend_{high,low}_iKxL_{s,u} A)
//             (iMxN.extend_{high,low}_iKxL_{s,u} B))
//
// It doesn't really matter what the inputs are, we can test this almost
// blindly.
//
// Unfortunately, we do not yet have i64x2.extend_* so we introduce a helper
// function to compute that.

function makeExtMulTest(wide, narrow, part, signed) {
    let widener = (wide == 'i64x2') ?
        `call $${wide}_extend_${part}_${narrow}_${signed}` :
        `${wide}.extend_${part}_${narrow}_${signed}`;
    return `
    (func (export "${wide}_extmul_${part}_${narrow}_${signed}")
      (v128.store (i32.const 0)
         (${wide}.extmul_${part}_${narrow}_${signed} (v128.load (i32.const 16))
                                                     (v128.load (i32.const 32))))
      (v128.store (i32.const 48)
         (${wide}.mul (${widener} (v128.load (i32.const 16)))
                      (${widener} (v128.load (i32.const 32))))))
`;
}

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $i64x2_extend_low_i32x4_s (param v128) (result v128)
      (i64x2.shr_s (i8x16.shuffle 16 16 16 16 0 1 2 3 16 16 16 16 4 5 6 7
                                  (local.get 0)
                                  (v128.const i32x4 0 0 0 0))
                   (i32.const 32)))
    (func $i64x2_extend_high_i32x4_s (param v128) (result v128)
      (i64x2.shr_s (i8x16.shuffle 16 16 16 16 8 9 10 11 16 16 16 16 12 13 14 15
                                  (local.get 0)
                                  (v128.const i32x4 0 0 0 0))
                   (i32.const 32)))
    (func $i64x2_extend_low_i32x4_u (param v128) (result v128)
      (i8x16.shuffle 0 1 2 3 16 16 16 16 4 5 6 7 16 16 16 16
                     (local.get 0)
                     (v128.const i32x4 0 0 0 0)))
    (func $i64x2_extend_high_i32x4_u (param v128) (result v128)
      (i8x16.shuffle 8 9 10 11 16 16 16 16 12 13 14 15 16 16 16 16
                     (local.get 0)
                     (v128.const i32x4 0 0 0 0)))
    ${makeExtMulTest('i64x2','i32x4','low','s')}
    ${makeExtMulTest('i64x2','i32x4','high','s')}
    ${makeExtMulTest('i64x2','i32x4','low','u')}
    ${makeExtMulTest('i64x2','i32x4','high','u')}
    ${makeExtMulTest('i32x4','i16x8','low','s')}
    ${makeExtMulTest('i32x4','i16x8','high','s')}
    ${makeExtMulTest('i32x4','i16x8','low','u')}
    ${makeExtMulTest('i32x4','i16x8','high','u')}
    ${makeExtMulTest('i16x8','i8x16','low','s')}
    ${makeExtMulTest('i16x8','i8x16','high','s')}
    ${makeExtMulTest('i16x8','i8x16','low','u')}
    ${makeExtMulTest('i16x8','i8x16','high','u')})`);

for ( let [ WideArray, NarrowArray ] of
      [ [ Int16Array, Int8Array ],
        [ Int32Array, Int16Array ],
        [ BigInt64Array, Int32Array ] ] ) {
    let narrowMem = new NarrowArray(ins.exports.mem.buffer);
    let narrowSrc0 = 16/NarrowArray.BYTES_PER_ELEMENT;
    let narrowSrc1 = 32/NarrowArray.BYTES_PER_ELEMENT;
    let wideMem = new WideArray(ins.exports.mem.buffer);
    let wideElems = 16/WideArray.BYTES_PER_ELEMENT;
    let wideRes0 = 0;
    let wideRes1 = 48/WideArray.BYTES_PER_ELEMENT;
    let zero = iota(wideElems).map(_ => 0);
    for ( let part of [ 'low', 'high' ] ) {
        for ( let signed of [ 's', 'u' ] ) {
            for ( let [a, b] of cross(NarrowArray.inputs) ) {
                set(wideMem, wideRes0, zero);
                set(wideMem, wideRes1, zero);
                set(narrowMem, narrowSrc0, a);
                set(narrowMem, narrowSrc1, b);
                let test = `${WideArray.layoutName}_extmul_${part}_${NarrowArray.layoutName}_${signed}`;
                ins.exports[test]();
                assertSame(get(wideMem, wideRes0, wideElems),
                           get(wideMem, wideRes1, wideElems));
            }
        }
    }
}

// Bitmask.  Ion constant folds, so test that too.
// This is to be merged into the existing bitmask tests in ad-hack.js.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "bitmask_i64x2") (result i32)
      (i64x2.bitmask (v128.load (i32.const 16))))
    (func (export "const_bitmask_i64x2") (result i32)
      (i64x2.bitmask (v128.const i64x2 0xff337f8012345678 0x0001984212345678))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var mem64 = new BigUint64Array(ins.exports.mem.buffer);

set(mem8, 16, iota(16).map((_) => 0));
assertEq(ins.exports.bitmask_i64x2(), 0);

set(mem64, 2, [0x8000000000000000n, 0x8000000000000000n]);
assertEq(ins.exports.bitmask_i64x2(), 3);

set(mem64, 2, [0x7FFFFFFFFFFFFFFFn, 0x7FFFFFFFFFFFFFFFn]);
assertEq(ins.exports.bitmask_i64x2(), 0);

set(mem64, 2, [0n, 0x8000000000000000n]);
assertEq(ins.exports.bitmask_i64x2(), 2);

set(mem64, 2, [0x8000000000000000n, 0n]);
assertEq(ins.exports.bitmask_i64x2(), 1);

assertEq(ins.exports.const_bitmask_i64x2(), 1);

// Widen low/high.
// This is to be merged into the existing widening tests in ad-hack.js.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "extend_low_i32x4_s")
      (v128.store (i32.const 0) (i64x2.extend_low_i32x4_s (v128.load (i32.const 16)))))
    (func (export "extend_high_i32x4_s")
      (v128.store (i32.const 0) (i64x2.extend_high_i32x4_s (v128.load (i32.const 16)))))
    (func (export "extend_low_i32x4_u")
      (v128.store (i32.const 0) (i64x2.extend_low_i32x4_u (v128.load (i32.const 16)))))
    (func (export "extend_high_i32x4_u")
      (v128.store (i32.const 0) (i64x2.extend_high_i32x4_u (v128.load (i32.const 16))))))`);

var mem32 = new Int32Array(ins.exports.mem.buffer);
var mem64 = new BigInt64Array(ins.exports.mem.buffer);
var mem64u = new BigUint64Array(ins.exports.mem.buffer);

var as = [205, 1, 192, 3].map((x) => x << 24);
set(mem32, 4, as);

ins.exports.extend_low_i32x4_s();
assertSame(get(mem64, 0, 2), iota(2).map((n) => BigInt(as[n])))

ins.exports.extend_high_i32x4_s();
assertSame(get(mem64, 0, 2), iota(2).map((n) => BigInt(as[n+2])));

ins.exports.extend_low_i32x4_u();
assertSame(get(mem64u, 0, 2), iota(2).map((n) => BigInt(as[n] >>> 0)));

ins.exports.extend_high_i32x4_u();
assertSame(get(mem64u, 0, 2), iota(2).map((n) => BigInt(as[n+2] >>> 0)));

// Saturating rounding q-format multiplication.
// This is to be moved into ad-hack.js

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "q15mulr_sat_s")
      (v128.store (i32.const 0) (i16x8.q15mulr_sat_s (v128.load (i32.const 16)) (v128.load (i32.const 32))))))`);

var mem16 = new Int16Array(ins.exports.mem.buffer);
for ( let [as, bs] of cross(Int16Array.inputs) ) {
    set(mem16, 8, as);
    set(mem16, 16, bs);
    ins.exports.q15mulr_sat_s();
    assertSame(get(mem16, 0, 8),
               iota(8).map((i) => signed_saturate((as[i] * bs[i] + 0x4000) >> 15, 16)));
}


// i64.all_true

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "i64_all_true") (result i32)
      (i64x2.all_true (v128.load (i32.const 16)) ) ) )`);

var mem32 = new Int32Array(ins.exports.mem.buffer);

set(mem32, 4, [0, 0, 0, 0]);
assertEq(0, ins.exports.i64_all_true());
set(mem32, 4, [1, 0, 0, 0]);
assertEq(0, ins.exports.i64_all_true());
set(mem32, 4, [1, 0, 0, 1]);
assertEq(1, ins.exports.i64_all_true());
set(mem32, 4, [0, 0, 10, 0]);
assertEq(0, ins.exports.i64_all_true());
set(mem32, 4, [0, -250, 1, 0]);
assertEq(1, ins.exports.i64_all_true());
set(mem32, 4, [-1, -1, -1, -1]);
assertEq(1, ins.exports.i64_all_true());

if (this.wasmSimdAnalysis && wasmCompileMode() == "ion") {
  const positive =
      wasmCompile(
          `(module
              (memory (export "mem") 1 1)
              (func $f (param v128) (result i32)
                  (if (result i32) (i64x2.all_true (local.get 0))
                      (i32.const 42)
                      (i32.const 37)))
              (func (export "run") (result i32)
                (call $f (v128.load (i32.const 16)))))`);
  assertEq(wasmSimdAnalysis(), "simd128-to-scalar-and-branch -> folded");

  const negative =
      wasmCompile(
          `(module
              (memory (export "mem") 1 1)
              (func $f (param v128) (result i32)
                  (if (result i32) (i32.eqz (i64x2.all_true (local.get 0)))
                      (i32.const 42)
                      (i32.const 37)))
              (func (export "run") (result i32)
                (call $f (v128.load (i32.const 16)))))`);
  assertEq(wasmSimdAnalysis(), "simd128-to-scalar-and-branch -> folded");

  for ( let inp of [[1n, 2n], [4n, 0n], [0n, 0n]]) {
      const all_true = inp.every(v => v != 0n)
      let mem = new BigInt64Array(positive.exports.mem.buffer);
      set(mem, 2, inp);
      assertEq(positive.exports.run(), all_true ? 42 : 37);

      mem = new BigInt64Array(negative.exports.mem.buffer);
      set(mem, 2, inp);
      assertEq(negative.exports.run(), all_true ? 37 : 42);
  }

  wasmCompile(`(module (func (result i32) (i64x2.all_true (v128.const i64x2 0 0))))`);
  assertEq(wasmSimdAnalysis(), "simd128-to-scalar -> constant folded");
}


// i64x2.eq and i64x2.ne

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "i64_eq")
      (v128.store (i32.const 0)
        (i64x2.eq (v128.load (i32.const 16)) (v128.load (i32.const 32))) ))
    (func (export "i64_ne")
      (v128.store (i32.const 0)
         (i64x2.ne (v128.load (i32.const 16)) (v128.load (i32.const 32))) )) )`);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);

set(mem64, 2, [0n, 1n, 0n, 1n]);
ins.exports.i64_eq();
assertSame(get(mem64, 0, 2), [-1n, -1n]);
ins.exports.i64_ne();
assertSame(get(mem64, 0, 2), [0n, 0n]);
set(mem64, 2, [0x0n, -1n, 0x100000000n, -1n]);
ins.exports.i64_eq();
assertSame(get(mem64, 0, 2), [0n, -1n]);
set(mem64, 2, [-1n, 0x0n, -1n, 0x100000000n]);
ins.exports.i64_ne();
assertSame(get(mem64, 0, 2), [0n, -1n]);


// i64x2.lt, i64x2.gt, i64x2.le, and i64.ge

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "i64_lt_s")
      (v128.store (i32.const 0)
        (i64x2.lt_s (v128.load (i32.const 16)) (v128.load (i32.const 32))) ))
    (func (export "i64_gt_s")
      (v128.store (i32.const 0)
        (i64x2.gt_s (v128.load (i32.const 16)) (v128.load (i32.const 32))) ))
    (func (export "i64_le_s")
      (v128.store (i32.const 0)
        (i64x2.le_s (v128.load (i32.const 16)) (v128.load (i32.const 32))) ))
    (func (export "i64_ge_s")
      (v128.store (i32.const 0)
        (i64x2.ge_s (v128.load (i32.const 16)) (v128.load (i32.const 32))) )) )`);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);

set(mem64, 2, [0n, 1n, 1n, 0n]);
ins.exports.i64_lt_s();
assertSame(get(mem64, 0, 2), [-1n, 0n]);
ins.exports.i64_gt_s();
assertSame(get(mem64, 0, 2), [0n, -1n]);
ins.exports.i64_le_s();
assertSame(get(mem64, 0, 2), [-1n, 0n]);
ins.exports.i64_ge_s();
assertSame(get(mem64, 0, 2), [0n, -1n]);

set(mem64, 2, [0n, -1n, -1n, 0n]);
ins.exports.i64_lt_s();
assertSame(get(mem64, 0, 2), [0n, -1n]);
ins.exports.i64_gt_s();
assertSame(get(mem64, 0, 2), [-1n, 0n]);
ins.exports.i64_le_s();
assertSame(get(mem64, 0, 2), [0n, -1n]);
ins.exports.i64_ge_s();
assertSame(get(mem64, 0, 2), [-1n, 0n]);

set(mem64, 2, [-2n, 2n, -1n, 1n]);
ins.exports.i64_lt_s();
assertSame(get(mem64, 0, 2), [-1n, 0n]);
ins.exports.i64_gt_s();
assertSame(get(mem64, 0, 2), [0n, -1n]);
ins.exports.i64_le_s();
assertSame(get(mem64, 0, 2), [-1n, 0n]);
ins.exports.i64_ge_s();
assertSame(get(mem64, 0, 2), [0n, -1n]);

set(mem64, 2, [-2n, 1n, -2n, 1n]);
ins.exports.i64_lt_s();
assertSame(get(mem64, 0, 2), [0n, 0n]);
ins.exports.i64_gt_s();
assertSame(get(mem64, 0, 2), [0n, 0n]);
ins.exports.i64_le_s();
assertSame(get(mem64, 0, 2), [-1n, -1n]);
ins.exports.i64_ge_s();
assertSame(get(mem64, 0, 2), [-1n, -1n]);


function wasmCompile(text) {
  return new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(text)))
}


// i64x2.abs

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "i64_abs")
      (v128.store (i32.const 0)
        (i64x2.abs (v128.load (i32.const 16))) )) )`);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);

set(mem64, 2, [-3n, 42n]);
ins.exports.i64_abs();
assertSame(get(mem64, 0, 2), [3n, 42n]);
set(mem64, 2, [0n, -0x8000000000000000n]);
ins.exports.i64_abs();
assertSame(get(mem64, 0, 2), [0n, -0x8000000000000000n]);


// Load lane

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    ${iota(16).map(i => `(func (export "load8_lane_${i}") (param i32)
      (v128.store (i32.const 0)
        (v128.load8_lane offset=0 ${i} (local.get 0) (v128.load (i32.const 16)))))
    `).join('')}
    ${iota(8).map(i => `(func (export "load16_lane_${i}") (param i32)
    (v128.store (i32.const 0)
      (v128.load16_lane offset=0 ${i} (local.get 0) (v128.load (i32.const 16)))))
    `).join('')}
    ${iota(4).map(i => `(func (export "load32_lane_${i}") (param i32)
    (v128.store (i32.const 0)
      (v128.load32_lane offset=0 ${i} (local.get 0) (v128.load (i32.const 16)))))
    `).join('')}
    ${iota(2).map(i => `(func (export "load64_lane_${i}") (param i32)
    (v128.store (i32.const 0)
      (v128.load64_lane offset=0 ${i} (local.get 0) (v128.load (i32.const 16)))))
    `).join('')}
    (func (export "load_lane_const_and_align")
      (v128.store (i32.const 0)
        (v128.load64_lane offset=32 1 (i32.const 1)
          (v128.load32_lane offset=32 1 (i32.const 3)
            (v128.load16_lane offset=32 0 (i32.const 5)
              (v128.load (i32.const 16)))))
      ))
  )`);

var mem8 = new Int8Array(ins.exports.mem.buffer);
var mem32 = new Int32Array(ins.exports.mem.buffer);
var mem64 = new BigInt64Array(ins.exports.mem.buffer);

var as = [0x12345678, 0x23456789, 0x3456789A, 0x456789AB];
set(mem32, 4, as); set(mem8, 32, [0xC2]);

ins.exports["load8_lane_0"](32);
assertSame(get(mem32, 0, 4), [0x123456C2, 0x23456789, 0x3456789A, 0x456789AB]);
ins.exports["load8_lane_1"](32);
assertSame(get(mem32, 0, 4), [0x1234C278, 0x23456789, 0x3456789A, 0x456789AB]);
ins.exports["load8_lane_2"](32);
assertSame(get(mem32, 0, 4), [0x12C25678, 0x23456789, 0x3456789A, 0x456789AB]);
ins.exports["load8_lane_3"](32);
assertSame(get(mem32, 0, 4), [0xC2345678|0, 0x23456789, 0x3456789A, 0x456789AB]);
ins.exports["load8_lane_4"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x234567C2, 0x3456789A, 0x456789AB]);
ins.exports["load8_lane_6"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x23C26789, 0x3456789A, 0x456789AB]);
ins.exports["load8_lane_9"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x23456789, 0x3456C29A, 0x456789AB]);
ins.exports["load8_lane_14"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x23456789, 0x3456789A, 0x45C289AB]);

set(mem8, 32, [0xC2, 0xD1]);

ins.exports["load16_lane_0"](32);
assertSame(get(mem32, 0, 4), [0x1234D1C2, 0x23456789, 0x3456789A, 0x456789AB]);
ins.exports["load16_lane_1"](32);
assertSame(get(mem32, 0, 4), [0xD1C25678|0, 0x23456789, 0x3456789A, 0x456789AB]);
ins.exports["load16_lane_2"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x2345D1C2, 0x3456789A, 0x456789AB]);
ins.exports["load16_lane_5"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x23456789, 0xD1C2789A|0, 0x456789AB]);
ins.exports["load16_lane_7"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x23456789, 0x3456789A, 0xD1C289AB|0]);

set(mem32, 8, [0x16B5C3D0]);

ins.exports["load32_lane_0"](32);
assertSame(get(mem32, 0, 4), [0x16B5C3D0, 0x23456789, 0x3456789A, 0x456789AB]);
ins.exports["load32_lane_1"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x16B5C3D0, 0x3456789A, 0x456789AB]);
ins.exports["load32_lane_2"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x23456789, 0x16B5C3D0, 0x456789AB]);
ins.exports["load32_lane_3"](32);
assertSame(get(mem32, 0, 4), [0x12345678, 0x23456789, 0x3456789A, 0x16B5C3D0]);

set(mem64, 4, [0x3300AA4416B5C3D0n]);

ins.exports["load64_lane_0"](32);
assertSame(get(mem64, 0, 2), [0x3300AA4416B5C3D0n, 0x456789AB3456789An]);
ins.exports["load64_lane_1"](32);
assertSame(get(mem64, 0, 2), [0x2345678912345678n, 0x3300AA4416B5C3D0n]);

// .. (mis)align load lane

var as = [0x12345678, 0x23456789, 0x3456789A, 0x456789AB];
set(mem32, 4, as); set(mem64, 4, [0x3300AA4416B5C3D0n, 0x300AA4416B5C3D03n]);

ins.exports["load16_lane_5"](33);
assertSame(get(mem32, 0, 4), [0x12345678,0x23456789,0xb5c3789a|0,0x456789ab]);
ins.exports["load32_lane_1"](34);
assertSame(get(mem32, 0, 4), [0x12345678, 0xaa4416b5|0,0x3456789a,0x456789ab]);
ins.exports["load64_lane_0"](35);
assertSame(get(mem64, 0, 2), [0x5c3d033300aa4416n, 0x456789ab3456789an]);

ins.exports["load_lane_const_and_align"]();
assertSame(get(mem32, 0, 4), [0x123400aa,0x00AA4416,0x4416b5c3,0x033300aa]);

// Store lane

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    ${iota(16).map(i => `(func (export "store8_lane_${i}") (param i32) (param i32)
      (v128.store8_lane ${i} (local.get 1) (v128.load (local.get 0))))
    `).join('')}
    ${iota(8).map(i => `(func (export "store16_lane_${i}") (param i32) (param i32)
      (v128.store16_lane ${i} (local.get 1) (v128.load (local.get 0))))
    `).join('')}
    ${iota(4).map(i => `(func (export "store32_lane_${i}") (param i32) (param i32)
      (v128.store32_lane ${i} (local.get 1) (v128.load (local.get 0))))
    `).join('')}
    ${iota(2).map(i => `(func (export "store64_lane_${i}") (param i32) (param i32)
      (v128.store64_lane ${i} (local.get 1) (v128.load (local.get 0))))
    `).join('')}
    (func (export "store_lane_const_and_align")
      (v128.store16_lane 1 (i32.const 33) (v128.load (i32.const 16)))
      (v128.store32_lane 2 (i32.const 37) (v128.load (i32.const 16)))
      (v128.store64_lane 0 (i32.const 47) (v128.load (i32.const 16)))
    ))`);


var mem8 = new Int8Array(ins.exports.mem.buffer);
var mem32 = new Int32Array(ins.exports.mem.buffer);
var mem64 = new BigInt64Array(ins.exports.mem.buffer);

var as = [0x12345678, 0x23456789, 0x3456789A, 0x456789AB];
set(mem32, 4, as); set(mem32, 0, [0x7799AA00, 42, 3, 0]);

ins.exports["store8_lane_0"](16, 0); assertSame(get(mem32, 0, 1), [0x7799AA78]);
ins.exports["store8_lane_1"](16, 0); assertSame(get(mem32, 0, 1), [0x7799AA56]);
ins.exports["store8_lane_2"](16, 0); assertSame(get(mem32, 0, 1), [0x7799AA34]);
ins.exports["store8_lane_3"](16, 0); assertSame(get(mem32, 0, 1), [0x7799AA12]);
ins.exports["store8_lane_5"](16, 0); assertSame(get(mem32, 0, 1), [0x7799AA67]);
ins.exports["store8_lane_7"](16, 0); assertSame(get(mem32, 0, 1), [0x7799AA23]);
ins.exports["store8_lane_8"](16, 0); assertSame(get(mem32, 0, 1), [0x7799AA9A]);
ins.exports["store8_lane_15"](16, 0); assertSame(get(mem32, 0, 1), [0x7799AA45]);

ins.exports["store16_lane_0"](16, 0); assertSame(get(mem32, 0, 1), [0x77995678]);
ins.exports["store16_lane_1"](16, 0); assertSame(get(mem32, 0, 1), [0x77991234]);
ins.exports["store16_lane_2"](16, 0); assertSame(get(mem32, 0, 1), [0x77996789]);
ins.exports["store16_lane_5"](16, 0); assertSame(get(mem32, 0, 1), [0x77993456]);
ins.exports["store16_lane_7"](16, 0); assertSame(get(mem32, 0, 1), [0x77994567]);

ins.exports["store32_lane_0"](16, 0); assertSame(get(mem32, 0, 2), [0x12345678, 42]);
ins.exports["store32_lane_1"](16, 0); assertSame(get(mem32, 0, 2), [0x23456789, 42]);
ins.exports["store32_lane_2"](16, 0); assertSame(get(mem32, 0, 2), [0x3456789A, 42]);
ins.exports["store32_lane_3"](16, 0); assertSame(get(mem32, 0, 2), [0x456789AB, 42]);

ins.exports["store64_lane_0"](16, 0); assertSame(get(mem64, 0, 2), [0x2345678912345678n, 3]);
ins.exports["store64_lane_1"](16, 0); assertSame(get(mem64, 0, 2), [0x456789AB3456789An, 3]);

// .. (mis)align store lane

var as = [0x12345678, 0x23456789, 0x3456789A, 0x456789AB];
set(mem32, 4, as); set(mem32, 0, [0x7799AA01, 42, 3, 0]);
ins.exports["store16_lane_1"](16, 1); assertSame(get(mem32, 0, 2), [0x77123401, 42]);
set(mem32, 0, [0x7799AA01, 42, 3, 0]);
ins.exports["store32_lane_1"](16, 2); assertSame(get(mem32, 0, 2), [0x6789AA01, 0x2345]);
set(mem32, 0, [0x7799AA01, 42, 5, 3]);
ins.exports["store64_lane_0"](16, 1);
assertSame(get(mem64, 0, 2), [0x4567891234567801n, 0x0300000023]);

set(mem32, 4, [
  0x12345678, 0x23456789, 0x3456789A, 0x456789AB,
  0x55AA55AA, 0xCC44CC44, 0x55AA55AA, 0xCC44CC44,
  0x55AA55AA, 0xCC44CC44, 0x55AA55AA, 0xCC44CC44,
]);
ins.exports["store_lane_const_and_align"]();
assertSame(get(mem32, 8, 8), [
  0x551234aa, 0x56789a44, 0x55aa5534, 0x7844cc44,
  0x89123456|0, 0xcc234567|0, 0x55aa55aa, 0xcc44cc44|0,
]);


// i8x16.popcnt

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "i8x16_popcnt")
      (v128.store (i32.const 0) (i8x16.popcnt (v128.load (i32.const 16)) )))
  )`);

var mem8 = new Int8Array(ins.exports.mem.buffer);

set(mem8, 16, [0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 3, -1, 0xF0, 0x11, 0xFE, 0x0F, 0xE]);
ins.exports.i8x16_popcnt();
assertSame(get(mem8, 0, 16), [0,1,1,1,1,1,1,1,1,2,8,4,2,7,4,3]);


/// Double-precision conversion instructions.
/// f64x2.convert_low_i32x4_{u,s} / i32x4.trunc_sat_f64x2_{u,s}_zero
/// f32x4.demote_f64x2_zero / f64x2.promote_low_f32x4

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "f64x2_convert_low_i32x4_s")
      (v128.store (i32.const 0) (f64x2.convert_low_i32x4_s (v128.load (i32.const 16)) )))
    (func (export "f64x2_convert_low_i32x4_u")
      (v128.store (i32.const 0) (f64x2.convert_low_i32x4_u (v128.load (i32.const 16)) )))

    (func (export "i32x4_trunc_sat_f64x2_s_zero")
      (v128.store (i32.const 0) (i32x4.trunc_sat_f64x2_s_zero (v128.load (i32.const 16)) )))
    (func (export "i32x4_trunc_sat_f64x2_u_zero")
      (v128.store (i32.const 0) (i32x4.trunc_sat_f64x2_u_zero (v128.load (i32.const 16)) )))

    (func (export "f32x4_demote_f64x2")
      (v128.store (i32.const 0) (f32x4.demote_f64x2_zero (v128.load (i32.const 16)) )))
    (func (export "f64x2_protomote_f32x4")
      (v128.store (i32.const 0) (f64x2.promote_low_f32x4 (v128.load (i32.const 16)) )))
  )`);

var mem32 = new Int32Array(ins.exports.mem.buffer);
var memU32 = new Uint32Array(ins.exports.mem.buffer);
var memF32 = new Float32Array(ins.exports.mem.buffer);
var memF64 = new Float64Array(ins.exports.mem.buffer);

// f64x2.convert_low_i32x4_u / f64x2.convert_low_i32x4_s

set(mem32, 4, [1, -2, 0, -2]);
ins.exports.f64x2_convert_low_i32x4_s();
assertSame(get(memF64, 0, 2), [1, -2]);
set(mem32, 4, [-1, 0, 5, -212312312]);
ins.exports.f64x2_convert_low_i32x4_s();
assertSame(get(memF64, 0, 2), [-1, 0]);

set(memU32, 4, [1, 4045646797, 4, 0]);
ins.exports.f64x2_convert_low_i32x4_u();
assertSame(get(memF64, 0, 2), [1, 4045646797]);
set(memU32, 4, [0, 2, 4, 3]);
ins.exports.f64x2_convert_low_i32x4_u();
assertSame(get(memF64, 0, 2), [0, 2]);

// i32x4.trunc_sat_f64x2_u_zero / i32x4.trunc_sat_f64x2_s_zero

set(memF64, 2, [0,0])
ins.exports.i32x4_trunc_sat_f64x2_s_zero();
assertSame(get(mem32, 0, 4), [0,0,0,0]);
ins.exports.i32x4_trunc_sat_f64x2_u_zero();
assertSame(get(memU32, 0, 4), [0,0,0,0]);

set(memF64, 2, [-1.23,65535.12])
ins.exports.i32x4_trunc_sat_f64x2_s_zero();
assertSame(get(mem32, 0, 4), [-1,65535,0,0]);
set(memF64, 2, [1.99,65535.12])
ins.exports.i32x4_trunc_sat_f64x2_u_zero();
assertSame(get(memU32, 0, 4), [1,65535,0,0]);

set(memF64, 2, [10e+100,-10e+100])
ins.exports.i32x4_trunc_sat_f64x2_s_zero();
assertSame(get(mem32, 0, 4), [0x7fffffff,-0x80000000,0,0]);
ins.exports.i32x4_trunc_sat_f64x2_u_zero();
assertSame(get(memU32, 0, 4), [0xffffffff,0,0,0]);

// f32x4.demote_f64x2_zero

set(memF64, 2, [1, 2])
ins.exports.f32x4_demote_f64x2();
assertSame(get(memF32, 0, 4), [1,2,0,0]);

set(memF64, 2, [-4e38, 4e38])
ins.exports.f32x4_demote_f64x2();
assertSame(get(memF32, 0, 4), [-Infinity,Infinity,0,0]);

set(memF64, 2, [-1e-46, 1e-46])
ins.exports.f32x4_demote_f64x2();
assertSame(get(memF32, 0, 4), [1/-Infinity,0,0,0]);

set(memF64, 2, [0, NaN])
ins.exports.f32x4_demote_f64x2();
assertSame(get(memF32, 0, 4), [0, NaN,0,0]);

set(memF64, 2, [Infinity, -Infinity])
ins.exports.f32x4_demote_f64x2();
assertSame(get(memF32, 0, 4), [Infinity, -Infinity,0,0]);

// f64x2.promote_low_f32x4

set(memF32, 4, [4, 3, 1, 2])
ins.exports.f64x2_protomote_f32x4();
assertSame(get(memF64, 0, 2), [4, 3]);

set(memF32, 4, [NaN, 0, 0, 0])
ins.exports.f64x2_protomote_f32x4();
assertSame(get(memF64, 0, 2), [NaN, 0]);

set(memF32, 4, [Infinity, -Infinity, 0, 0])
ins.exports.f64x2_protomote_f32x4();
assertSame(get(memF64, 0, 2), [Infinity, -Infinity]);


// i16x8.extadd_pairwise_i8x16_{s,u} / i32x4.extadd_pairwise_i16x8_{s,u}

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "i16x8_extadd_pairwise_i8x16_s")
      (v128.store (i32.const 0) (i16x8.extadd_pairwise_i8x16_s (v128.load (i32.const 16)) )))
    (func (export "i16x8_extadd_pairwise_i8x16_u")
      (v128.store (i32.const 0) (i16x8.extadd_pairwise_i8x16_u (v128.load (i32.const 16)) )))

    (func (export "i32x4_extadd_pairwise_i16x8_s")
      (v128.store (i32.const 0) (i32x4.extadd_pairwise_i16x8_s (v128.load (i32.const 16)) )))
    (func (export "i32x4_extadd_pairwise_i16x8_u")
      (v128.store (i32.const 0) (i32x4.extadd_pairwise_i16x8_u (v128.load (i32.const 16)) )))
  )`);

var mem8 = new Int8Array(ins.exports.mem.buffer);
var memU8 = new Uint8Array(ins.exports.mem.buffer);
var mem16 = new Int16Array(ins.exports.mem.buffer);
var memU16 = new Uint16Array(ins.exports.mem.buffer);
var mem32 = new Int32Array(ins.exports.mem.buffer);
var memU32 = new Uint32Array(ins.exports.mem.buffer);

set(mem8, 16, [0, 0, 1, 1, 2, -2, 0, 42, 1, -101, 101, -1, 127, 125, -1, -2]);
ins.exports.i16x8_extadd_pairwise_i8x16_s();
assertSame(get(mem16, 0, 8), [0, 2, 0, 42, -100, 100, 252, -3]);

set(memU8, 16, [0, 0, 1, 1, 2, 255, 0, 42, 0, 255, 254, 0, 127, 125, 255, 255]);
ins.exports.i16x8_extadd_pairwise_i8x16_u();
assertSame(get(memU16, 0, 8), [0, 2, 257, 42, 255, 254, 252, 510]);

set(mem16, 8, [0, 0, 1, 1, 2, -2, -1, -2]);
ins.exports.i32x4_extadd_pairwise_i16x8_s();
assertSame(get(mem32, 0, 4), [0, 2, 0, -3]);
set(mem16, 8, [0, 42, 1, -32760, 32766, -1, 32761, 32762]);
ins.exports.i32x4_extadd_pairwise_i16x8_s();
assertSame(get(mem32, 0, 4), [42, -32759, 32765, 65523]);

set(memU16, 8, [0, 0, 1, 1, 2, 65535, 65535, 65535]);
ins.exports.i32x4_extadd_pairwise_i16x8_u();
assertSame(get(memU32, 0, 4), [0, 2, 65537, 131070]);
set(memU16, 8, [0, 42, 0, 65535, 65534, 0, 32768, 32765]);
ins.exports.i32x4_extadd_pairwise_i16x8_u();
assertSame(get(memU32, 0, 4), [42, 65535, 65534, 65533]);
