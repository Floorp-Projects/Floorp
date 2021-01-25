// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode().indexOf("cranelift") >= 0; include:../tests/wasm/simd/ad-hack-preamble.js

// New instructions, not yet supported by cranelift

// Widening multiplication.
// This is to be moved into ad-hack.js
//
//   (iMxN.extmul_{high,low}_iKxL_{s,u} A B)
//
// is equivalent to
//
//   (iMxN.mul (iMxN.widen_{high,low}_iKxL_{s,u} A)
//             (iMxN.widen_{high,low}_iKxL_{s,u} B))
//
// It doesn't really matter what the inputs are, we can test this almost
// blindly.
//
// Unfortunately, we do not yet have i64x2.widen_* so we introduce a helper
// function to compute that.

function makeExtMulTest(wide, narrow, part, signed) {
    let widener = (wide == 'i64x2') ?
        `call $${wide}_widen_${part}_${narrow}_${signed}` :
        `${wide}.widen_${part}_${narrow}_${signed}`;
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
    (func $i64x2_widen_low_i32x4_s (param v128) (result v128)
      (i64x2.shr_s (i8x16.shuffle 16 16 16 16 0 1 2 3 16 16 16 16 4 5 6 7
                                  (local.get 0)
                                  (v128.const i32x4 0 0 0 0))
                   (i32.const 32)))
    (func $i64x2_widen_high_i32x4_s (param v128) (result v128)
      (i64x2.shr_s (i8x16.shuffle 16 16 16 16 8 9 10 11 16 16 16 16 12 13 14 15
                                  (local.get 0)
                                  (v128.const i32x4 0 0 0 0))
                   (i32.const 32)))
    (func $i64x2_widen_low_i32x4_u (param v128) (result v128)
      (i8x16.shuffle 0 1 2 3 16 16 16 16 4 5 6 7 16 16 16 16
                     (local.get 0)
                     (v128.const i32x4 0 0 0 0)))
    (func $i64x2_widen_high_i32x4_u (param v128) (result v128)
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
    (func (export "widen_low_i32x4_s")
      (v128.store (i32.const 0) (i64x2.widen_low_i32x4_s (v128.load (i32.const 16)))))
    (func (export "widen_high_i32x4_s")
      (v128.store (i32.const 0) (i64x2.widen_high_i32x4_s (v128.load (i32.const 16)))))
    (func (export "widen_low_i32x4_u")
      (v128.store (i32.const 0) (i64x2.widen_low_i32x4_u (v128.load (i32.const 16)))))
    (func (export "widen_high_i32x4_u")
      (v128.store (i32.const 0) (i64x2.widen_high_i32x4_u (v128.load (i32.const 16))))))`);

var mem32 = new Int32Array(ins.exports.mem.buffer);
var mem64 = new BigInt64Array(ins.exports.mem.buffer);
var mem64u = new BigUint64Array(ins.exports.mem.buffer);

var as = [205, 1, 192, 3].map((x) => x << 24);
set(mem32, 4, as);

ins.exports.widen_low_i32x4_s();
assertSame(get(mem64, 0, 2), iota(2).map((n) => BigInt(as[n])))

ins.exports.widen_high_i32x4_s();
assertSame(get(mem64, 0, 2), iota(2).map((n) => BigInt(as[n+2])));

ins.exports.widen_low_i32x4_u();
assertSame(get(mem64u, 0, 2), iota(2).map((n) => BigInt(as[n] >>> 0)));

ins.exports.widen_high_i32x4_u();
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
