function testValid(code) {
    assertEq(WebAssembly.validate(wasmTextToBinary(code)), true);
}

function testInvalid(code) {
    assertEq(WebAssembly.validate(wasmTextToBinary(code)), false);
}

// v128 -> v128

for (let op of [
    'i8x16.neg',
    'i8x16.abs',
    'i16x8.neg',
    'i16x8.abs',
    'i16x8.widen_low_i8x16_s',
    'i16x8.widen_high_i8x16_s',
    'i16x8.widen_low_i8x16_u',
    'i16x8.widen_high_i8x16_u',
    'i32x4.neg',
    'i32x4.abs',
    'i32x4.widen_low_i16x8_s',
    'i32x4.widen_high_i16x8_s',
    'i32x4.widen_low_i16x8_u',
    'i32x4.widen_high_i16x8_u',
    'i32x4.trunc_sat_f32x4_s',
    'i32x4.trunc_sat_f32x4_u',
    'i64x2.neg',
    'f32x4.abs',
    'f32x4.neg',
    'f32x4.sqrt',
    'f32x4.convert_i32x4_s',
    'f32x4.convert_i32x4_s',
    'f64x2.abs',
    'f64x2.neg',
    'f64x2.sqrt',
    'v128.not'])
{
    testValid(`(module
                 (func (param v128) (result v128)
                   (${op} (local.get 0))))`);
}

for (let [prefix, result, suffix] of [['i8x16', 'i32', '_s'],
                                      ['i8x16', 'i32', '_u'],
                                      ['i16x8', 'i32', '_s'],
                                      ['i16x8', 'i32', '_u'],
                                      ['i32x4', 'i32', ''],
                                      ['i64x2', 'i64', ''],
                                      ['f32x4', 'f32', ''],
                                      ['f64x2', 'f64', '']])
{
    testValid(`(module
                 (func (param v128) (result ${result})
                   (${prefix}.extract_lane${suffix} 1 (local.get 0))))`);
}

// The wat parser accepts small out-of-range lane indices, but they must be
// caught in validation.

testInvalid(
    `(module
       (func (param v128) (result i32)
         (i8x16.extract_lane_u 16 (local.get 0))))`);

// (v128, v128) -> v128

for (let op of [
    'i8x16.eq',
    'i8x16.ne',
    'i8x16.lt_s',
    'i8x16.lt_u',
    'i8x16.gt_s',
    'i8x16.gt_u',
    'i8x16.le_s',
    'i8x16.le_u',
    'i8x16.ge_s',
    'i8x16.ge_u',
    'i16x8.eq',
    'i16x8.ne',
    'i16x8.lt_s',
    'i16x8.lt_u',
    'i16x8.gt_s',
    'i16x8.gt_u',
    'i16x8.le_s',
    'i16x8.le_u',
    'i16x8.ge_s',
    'i16x8.ge_u',
    'i32x4.eq',
    'i32x4.ne',
    'i32x4.lt_s',
    'i32x4.lt_u',
    'i32x4.gt_s',
    'i32x4.gt_u',
    'i32x4.le_s',
    'i32x4.le_u',
    'i32x4.ge_s',
    'i32x4.ge_u',
    'f32x4.eq',
    'f32x4.ne',
    'f32x4.lt',
    'f32x4.gt',
    'f32x4.le',
    'f32x4.ge',
    'f64x2.eq',
    'f64x2.ne',
    'f64x2.lt',
    'f64x2.gt',
    'f64x2.le',
    'f64x2.ge',
    'v128.and',
    'v128.or',
    'v128.xor',
    'v128.andnot',
    'i8x16.avgr_u',
    'i16x8.avgr_u',
    'i8x16.add',
    'i8x16.add_saturate_s',
    'i8x16.add_saturate_u',
    'i8x16.sub',
    'i8x16.sub_saturate_s',
    'i8x16.sub_saturate_u',
    'i8x16.min_s',
    'i8x16.max_s',
    'i8x16.min_u',
    'i8x16.max_u',
    'i16x8.add',
    'i16x8.add_saturate_s',
    'i16x8.add_saturate_u',
    'i16x8.sub',
    'i16x8.sub_saturate_s',
    'i16x8.sub_saturate_u',
    'i16x8.mul',
    'i16x8.min_s',
    'i16x8.max_s',
    'i16x8.min_u',
    'i16x8.max_u',
    'i32x4.add',
    'i32x4.sub',
    'i32x4.mul',
    'i32x4.min_s',
    'i32x4.max_s',
    'i32x4.min_u',
    'i32x4.max_u',
    'i64x2.add',
    'i64x2.sub',
    'i64x2.mul',
    'f32x4.add',
    'f32x4.sub',
    'f32x4.mul',
    'f32x4.div',
    'f32x4.min',
    'f32x4.max',
    'f64x2.add',
    'f64x2.sub',
    'f64x2.mul',
    'f64x2.div',
    'f64x2.min',
    'f64x2.max',
    'i8x16.narrow_i16x8_s',
    'i8x16.narrow_i16x8_u',
    'i16x8.narrow_i32x4_s',
    'i16x8.narrow_i32x4_u',
    'v8x16.swizzle'])
{
    testValid(`(module
                 (func (param v128) (param v128) (result v128)
                   (${op} (local.get 0) (local.get 1))))`);
}

testValid(`(module
             (func (param v128) (param v128) (result v128)
               (v8x16.shuffle 0 16 1 17 2 18 3 19 4 20 5 21 6 22 7 23 (local.get 0) (local.get 1))))`);

assertErrorMessage(() => testValid(
    `(module
       (func (param v128) (param v128) (result v128)
         (v8x16.shuffle 0 16 1 17 2 18 3 19 4 20 5 21 6 22 7 (local.get 0) (local.get 1))))`),
                   SyntaxError,
                   /expected a u8/);

// (v128, i32) -> v128

for (let op of [
    'i8x16.shl',
    'i8x16.shr_s',
    'i8x16.shr_u',
    'i16x8.shl',
    'i16x8.shr_s',
    'i16x8.shr_u',
    'i32x4.shl',
    'i32x4.shr_s',
    'i32x4.shr_u',
    'i64x2.shl',
    'i64x2.shr_s',
    'i64x2.shr_u'])
{
    testValid(`(module
                 (func (param v128) (param i32) (result v128)
                   (${op} (local.get 0) (local.get 1))))`);
}

// v128 -> i32

for (let op of [
    'i8x16.any_true',
    'i8x16.all_true',
    'i16x8.any_true',
    'i16x8.all_true',
    'i32x4.any_true',
    'i32x4.all_true'])
{
    testValid(`(module
                 (func (param v128) (result i32)
                   (${op} (local.get 0))))`);
}

// T -> V128

for (let [op, input] of [
    ['i8x16.splat', 'i32'],
    ['i16x8.splat', 'i32'],
    ['i32x4.splat', 'i32'],
    ['i64x2.splat', 'i64'],
    ['f32x4.splat', 'f32'],
    ['f64x2.splat', 'f64']])
{
    testValid(`(module
                 (func (param ${input}) (result v128)
                   (${op} (local.get 0))))`);
}

// i32 -> v128

for (let op of [
    'v128.load',
    'v8x16.load_splat',
    'v16x8.load_splat',
    'v32x4.load_splat',
    'v64x2.load_splat',
    'i16x8.load8x8_s',
    'i16x8.load8x8_u',
    'i32x4.load16x4_s',
    'i32x4.load16x4_u',
    'i64x2.load32x2_s',
    'i64x2.load32x2_u'])
{
    testValid(`(module
                 (memory 1 1)
                 (func (param i32) (result v128)
                   (${op} (local.get 0))))`);
}

testValid(`(module
             (func (result v128)
               (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
             (func (result v128)
               (v128.const i16x8 0 1 2 3 4 5 6 7))
             (func (result v128)
               (v128.const i32x4 0 1 2 3))
             (func (result v128)
               (v128.const i64x2 0 1))
             (func (result v128)
               (v128.const f32x4 0 1 2 3))
             (func (result v128)
               (v128.const f32x4 0.5 1.5 2.5 3.5))
             (func (result v128)
               (v128.const f64x2 0 1))
             (func (result v128)
               (v128.const f64x2 0.5 1.5)))`);

assertErrorMessage(() => testValid(
    `(module
       (func (result v128)
         (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14)))`),
                   SyntaxError,
                   /expected a i8/);

assertErrorMessage(() => testValid(
    `(module
       (func (result v128)
         (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 256 15)))`),
                   SyntaxError,
                   /invalid i8 number/);

assertErrorMessage(() => testValid(
    `(module
       (func (result v128)
         (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 3.14 15)))`),
                   SyntaxError,
                   /expected a i8/);

assertErrorMessage(() => testValid(
    `(module
       (func (result v128)
         (v128.const f32x4 0.5 1.5 2.5))`),
                   SyntaxError,
                   /expected a float/);

assertErrorMessage(() => testValid(
    `(module
       (func (result v128)
         (v128.const i8x8 0 1 2 3 4 5 6 7)))`),
                   SyntaxError,
                   /expected one of/);

// v128 -> ()

testValid(`(module
             (memory 1 1)
             (func (param i32) (param v128)
               (v128.store (local.get 0) (local.get 1))))`);

// (v128, v128, v128) -> v128

testValid(`(module
             (func (param v128) (param v128) (param v128) (result v128)
               (v128.bitselect (local.get 0) (local.get 1) (local.get 2))))`);

// (v128, t) -> v128

for (let [prefix, input] of [['i8x16', 'i32'],
                             ['i16x8', 'i32'],
                             ['i32x4', 'i32'],
                             ['i64x2', 'i64'],
                             ['f32x4', 'f32'],
                             ['f64x2', 'f64']])
{
    testValid(`(module
                 (func (param v128) (param ${input}) (result v128)
                   (${prefix}.replace_lane 1 (local.get 0) (local.get 1))))`);
}

testInvalid(
    `(module
       (func (param v128) (param i32) (result v128)
         (i8x16.replace_lane 16 (local.get 0) (local.get 1))))`);

// Global variables

testValid(`(module
             (global $g (mut v128) (v128.const f32x4 1 2 3 4)))`);

testValid(`(module
             (global $g (import "m" "g") v128)
             (global $h (mut v128) (global.get $g)))`);

testValid(`(module
             (global $g (export "g") v128 (v128.const f32x4 1 2 3 4)))`);

testValid(`(module
             (global $g (export "g") (mut v128) (v128.const f32x4 1 2 3 4)))`);

// Imports, exports, calls

testValid(`(module
             (import "m" "g" (func (param v128) (result v128)))
             (func (export "f") (param v128) (result v128)
               (f64x2.add (local.get 0) (v128.const f64x2 1 2))))`);

testValid(`(module
             (func $f (param v128) (result v128)
               (i8x16.neg (local.get 0)))
             (func $g (export "g") (param v128) (result v128)
               (call $f (local.get 0))))`);
