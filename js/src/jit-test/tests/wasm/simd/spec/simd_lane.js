var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i8x16_extract_lane_s-first" ) ( param v128 ) ( result i32 ) ( i8x16.extract_lane_s 0 ( local.get 0 ) ) ) ( func ( export "i8x16_extract_lane_s-last" ) ( param v128 ) ( result i32 ) ( i8x16.extract_lane_s 15 ( local.get 0 ) ) ) ( func ( export "i8x16_extract_lane_u-first" ) ( param v128 ) ( result i32 ) ( i8x16.extract_lane_u 0 ( local.get 0 ) ) ) ( func ( export "i8x16_extract_lane_u-last" ) ( param v128 ) ( result i32 ) ( i8x16.extract_lane_u 15 ( local.get 0 ) ) ) ( func ( export "i16x8_extract_lane_s-first" ) ( param v128 ) ( result i32 ) ( i16x8.extract_lane_s 0 ( local.get 0 ) ) ) ( func ( export "i16x8_extract_lane_s-last" ) ( param v128 ) ( result i32 ) ( i16x8.extract_lane_s 7 ( local.get 0 ) ) ) ( func ( export "i16x8_extract_lane_u-first" ) ( param v128 ) ( result i32 ) ( i16x8.extract_lane_u 0 ( local.get 0 ) ) ) ( func ( export "i16x8_extract_lane_u-last" ) ( param v128 ) ( result i32 ) ( i16x8.extract_lane_u 7 ( local.get 0 ) ) ) ( func ( export "i32x4_extract_lane-first" ) ( param v128 ) ( result i32 ) ( i32x4.extract_lane 0 ( local.get 0 ) ) ) ( func ( export "i32x4_extract_lane-last" ) ( param v128 ) ( result i32 ) ( i32x4.extract_lane 3 ( local.get 0 ) ) ) ( func ( export "f32x4_extract_lane-first" ) ( param v128 ) ( result f32 ) ( f32x4.extract_lane 0 ( local.get 0 ) ) ) ( func ( export "f32x4_extract_lane-last" ) ( param v128 ) ( result f32 ) ( f32x4.extract_lane 3 ( local.get 0 ) ) ) ( func ( export "i8x16_replace_lane-first" ) ( param v128 i32 ) ( result v128 ) ( i8x16.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i8x16_replace_lane-last" ) ( param v128 i32 ) ( result v128 ) ( i8x16.replace_lane 15 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i16x8_replace_lane-first" ) ( param v128 i32 ) ( result v128 ) ( i16x8.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i16x8_replace_lane-last" ) ( param v128 i32 ) ( result v128 ) ( i16x8.replace_lane 7 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i32x4_replace_lane-first" ) ( param v128 i32 ) ( result v128 ) ( i32x4.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i32x4_replace_lane-last" ) ( param v128 i32 ) ( result v128 ) ( i32x4.replace_lane 3 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "f32x4_replace_lane-first" ) ( param v128 f32 ) ( result v128 ) ( f32x4.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "f32x4_replace_lane-last" ) ( param v128 f32 ) ( result v128 ) ( f32x4.replace_lane 3 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i64x2_extract_lane-first" ) ( param v128 ) ( result i64 ) ( i64x2.extract_lane 0 ( local.get 0 ) ) ) ( func ( export "i64x2_extract_lane-last" ) ( param v128 ) ( result i64 ) ( i64x2.extract_lane 1 ( local.get 0 ) ) ) ( func ( export "f64x2_extract_lane-first" ) ( param v128 ) ( result f64 ) ( f64x2.extract_lane 0 ( local.get 0 ) ) ) ( func ( export "f64x2_extract_lane-last" ) ( param v128 ) ( result f64 ) ( f64x2.extract_lane 1 ( local.get 0 ) ) ) ( func ( export "i64x2_replace_lane-first" ) ( param v128 i64 ) ( result v128 ) ( i64x2.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i64x2_replace_lane-last" ) ( param v128 i64 ) ( result v128 ) ( i64x2.replace_lane 1 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "f64x2_replace_lane-first" ) ( param v128 f64 ) ( result v128 ) ( f64x2.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "f64x2_replace_lane-last" ) ( param v128 f64 ) ( result v128 ) ( f64x2.replace_lane 1 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "v8x16_swizzle" ) ( param v128 v128 ) ( result v128 ) ( v8x16.swizzle ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "v8x16_shuffle-1" ) ( param v128 v128 ) ( result v128 ) ( v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "v8x16_shuffle-2" ) ( param v128 v128 ) ( result v128 ) ( v8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "v8x16_shuffle-3" ) ( param v128 v128 ) ( result v128 ) ( v8x16.shuffle 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "v8x16_shuffle-4" ) ( param v128 v128 ) ( result v128 ) ( v8x16.shuffle 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "v8x16_shuffle-5" ) ( param v128 v128 ) ( result v128 ) ( v8x16.shuffle 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "v8x16_shuffle-6" ) ( param v128 v128 ) ( result v128 ) ( v8x16.shuffle 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "v8x16_shuffle-7" ) ( param v128 v128 ) ( result v128 ) ( v8x16.shuffle 0 0 0 0 0 0 0 0 16 16 16 16 16 16 16 16 ( local.get 0 ) ( local.get 1 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 127 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 127 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0x7f 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 127 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 255 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xff 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_u-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 255 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 255 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_u-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xff 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 255 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_s-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -128 )))
(local.set $expected ( i32.const -128 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_s-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0x80 )))
(local.set $expected ( i32.const -128 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 )))
(local.set $expected ( i32.const 255 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0xff )))
(local.set $expected ( i32.const 255 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -128 )))
(local.set $expected ( i32.const 128 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0x80 )))
(local.set $expected ( i32.const 128 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 32767 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 32767 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 32767 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 65535 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xffff 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 012_345 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 12345 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -0x0_1234 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const -0x1234 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 65535 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 65535 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xffff 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 65535 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 012_345 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 12345 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -0x0_1234 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 60876 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 -32768 )))
(local.set $expected ( i32.const -32768 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0x8000 )))
(local.set $expected ( i32.const -32768 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 06_789 )))
(local.set $expected ( i32.const 6789 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 -0x0_6789 )))
(local.set $expected ( i32.const -0x6789 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 -1 )))
(local.set $expected ( i32.const 65535 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0xffff )))
(local.set $expected ( i32.const 65535 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 -32768 )))
(local.set $expected ( i32.const 32768 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0x8000 )))
(local.set $expected ( i32.const 32768 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 06_789 )))
(local.set $expected ( i32.const 6789 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 -0x0_6789 )))
(local.set $expected ( i32.const 39031 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 2147483647 0 0 0 )))
(local.set $expected ( i32.const 2147483647 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0x7fffffff 0 0 0 )))
(local.set $expected ( i32.const 2147483647 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 4294967295 0 0 0 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xffffffff 0 0 0 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 01_234_567_890 0 0 0 )))
(local.set $expected ( i32.const 1234567890 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-first" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -0x0_1234_5678 0 0 0 )))
(local.set $expected ( i32.const -0x12345678 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 -2147483648 )))
(local.set $expected ( i32.const -2147483648 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0x80000000 )))
(local.set $expected ( i32.const -2147483648 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 -1 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0xffffffff )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0_987_654_321 )))
(local.set $expected ( i32.const 987654321 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane-last" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 -0x0_1234_5678 )))
(local.set $expected ( i32.const -0x12345678 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-first" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 9223372036854775807 0 )))
(local.set $expected ( i64.const 9223372036854775807 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-first" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0x7ffffffffffffffe 0 )))
(local.set $expected ( i64.const 0x7ffffffffffffffe ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-first" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 0 )))
(local.set $expected ( i64.const -1 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-first" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0 )))
(local.set $expected ( i64.const -1 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-first" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 01_234_567_890_123_456_789 0 )))
(local.set $expected ( i64.const 1234567890123456789 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-first" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0x0_1234_5678_90AB_cdef 0 )))
(local.set $expected ( i64.const 0x1234567890abcdef ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0 9223372036854775808 )))
(local.set $expected ( i64.const -9223372036854775808 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0 0x8000000000000000 )))
(local.set $expected ( i64.const -0x8000000000000000 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0 0x8000000000000000 )))
(local.set $expected ( i64.const 0x8000000000000000 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0x7f )))
(local.set $expected ( i64.const 9223372036854775807 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0x8000 )))
(local.set $expected ( i64.const -9223372036854775808 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0xffffffff 0x7fffffff )))
(local.set $expected ( i64.const 9223372036854775807 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 -inf +inf )))
(local.set $expected ( i64.const 0x7ff0000000000000 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0 01_234_567_890_123_456_789 )))
(local.set $expected ( i64.const 1234567890123456789 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane-last" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0 0x0_1234_5678_90AB_cdef )))
(local.set $expected ( i64.const 0x1234567890abcdef ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-first" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 -5.0 0.0 0.0 0.0 )))
(local.set $expected ( f32.const -5.0 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-first" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 1e38 0.0 0.0 0.0 )))
(local.set $expected ( f32.const 1e38 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-first" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep127 0.0 0.0 0.0 )))
(local.set $expected ( f32.const 0x1.fffffep127 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-first" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0x1p127 0.0 0.0 0.0 )))
(local.set $expected ( f32.const 0x1p127 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-first" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 inf 0.0 0.0 0.0 )))
(local.set $expected ( f32.const inf ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-first" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)
(local $result1 i32) (local $expected1 i32)
(local.set $result (call $f ( v128.const f32x4 nan inf 0.0 0.0 )))
(local.set $expected ( f32.const nan ))

(local.set $result1 (i32.and (i32.reinterpret_f32 (local.get $result)) (i32.const  0x7FC00000)))
(local.set $expected1 (i32.and (i32.reinterpret_f32 (local.get $expected)) (i32.const  0x7FC00000)))

(local.set $cmpresult (i32.eq (local.get $result1) (local.get $expected1)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-first" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0123456789.0123456789e+019 0.0 0.0 0.0 )))
(local.set $expected ( f32.const 123456789.0123456789e+019 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-first" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0x0123456789ABCDEF.019aFp-019 0.0 0.0 0.0 )))
(local.set $expected ( f32.const 0x123456789ABCDEF.019aFp-019 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-last" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 -1e38 )))
(local.set $expected ( f32.const -1e38 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-last" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 -0x1.fffffep127 )))
(local.set $expected ( f32.const -0x1.fffffep127 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-last" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 -0x1p127 )))
(local.set $expected ( f32.const -0x1p127 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-last" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 -inf )))
(local.set $expected ( f32.const -inf ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-last" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)
(local $result1 i32) (local $expected1 i32)
(local.set $result (call $f ( v128.const f32x4 0.0 0.0 -inf nan )))
(local.set $expected ( f32.const nan ))

(local.set $result1 (i32.and (i32.reinterpret_f32 (local.get $result)) (i32.const  0x7FC00000)))
(local.set $expected1 (i32.and (i32.reinterpret_f32 (local.get $expected)) (i32.const  0x7FC00000)))

(local.set $cmpresult (i32.eq (local.get $result1) (local.get $expected1)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-last" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0123456789. )))
(local.set $expected ( f32.const 123456789.0 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane-last" (func $f (param v128) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0x0123456789ABCDEF. )))
(local.set $expected ( f32.const 0x123456789ABCDEF.0p0 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 -1.5 0.0 )))
(local.set $expected ( f64.const -1.5 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 1.5 0.0 )))
(local.set $expected ( f64.const 1.5 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 -1.7976931348623157e-308 0x0p+0 )))
(local.set $expected ( f64.const -1.7976931348623157e-308 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 1.7976931348623157e-308 0x0p-0 )))
(local.set $expected ( f64.const 1.7976931348623157e-308 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp-1023 0x0p+0 )))
(local.set $expected ( f64.const -0x1.fffffffffffffp-1023 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp-1023 0x0p-0 )))
(local.set $expected ( f64.const 0x1.fffffffffffffp-1023 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 -inf 0.0 )))
(local.set $expected ( f64.const -inf ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 inf 0.0 )))
(local.set $expected ( f64.const inf ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)
(local $result1 i64) (local $expected1 i64)
(local.set $result (call $f ( v128.const f64x2 -nan -0.0 )))
(local.set $expected ( f64.const -nan ))

(local.set $result1 (i64.and (i64.reinterpret_f64 (local.get $result)) (i64.const  0x7FF80000)))
(local.set $expected1 (i64.and (i64.reinterpret_f64 (local.get $expected)) (i64.const  0x7FF80000)))

(local.set $cmpresult (i64.eq (local.get $result1) (local.get $expected1)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)
(local $result1 i64) (local $expected1 i64)
(local.set $result (call $f ( v128.const f64x2 nan 0.0 )))
(local.set $expected ( f64.const nan ))

(local.set $result1 (i64.and (i64.reinterpret_f64 (local.get $result)) (i64.const  0x7FF80000)))
(local.set $expected1 (i64.and (i64.reinterpret_f64 (local.get $expected)) (i64.const  0x7FF80000)))

(local.set $cmpresult (i64.eq (local.get $result1) (local.get $expected1)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0123456789.0123456789e+019 0.0 )))
(local.set $expected ( f64.const 123456789.0123456789e+019 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-first" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0x0123456789ABCDEFabcdef.0123456789ABCDEFabcdefp-019 0.0 )))
(local.set $expected ( f64.const 0x123456789ABCDEFabcdef.0123456789ABCDEFabcdefp-019 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0.0 2.25 )))
(local.set $expected ( f64.const 2.25 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0.0 -2.25 )))
(local.set $expected ( f64.const -2.25 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0x0p-0 -1.7976931348623157e+308 )))
(local.set $expected ( f64.const -1.7976931348623157e+308 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 1.7976931348623157e+308 )))
(local.set $expected ( f64.const 1.7976931348623157e+308 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0x0p-0 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( f64.const -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x1.fffffffffffffp+1023 )))
(local.set $expected ( f64.const 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 -0.0 -inf )))
(local.set $expected ( f64.const -inf ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0.0 inf )))
(local.set $expected ( f64.const inf ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)
(local $result1 i64) (local $expected1 i64)
(local.set $result (call $f ( v128.const f64x2 -0.0 -nan )))
(local.set $expected ( f64.const -nan ))

(local.set $result1 (i64.and (i64.reinterpret_f64 (local.get $result)) (i64.const  0x7FF80000)))
(local.set $expected1 (i64.and (i64.reinterpret_f64 (local.get $expected)) (i64.const  0x7FF80000)))

(local.set $cmpresult (i64.eq (local.get $result1) (local.get $expected1)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)
(local $result1 i64) (local $expected1 i64)
(local.set $result (call $f ( v128.const f64x2 0.0 nan )))
(local.set $expected ( f64.const nan ))

(local.set $result1 (i64.and (i64.reinterpret_f64 (local.get $result)) (i64.const  0x7FF80000)))
(local.set $expected1 (i64.and (i64.reinterpret_f64 (local.get $expected)) (i64.const  0x7FF80000)))

(local.set $cmpresult (i64.eq (local.get $result1) (local.get $expected1)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0.0 0123456789. )))
(local.set $expected ( f64.const 123456789.0 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0.0 0x0123456789ABCDEFabcdef. )))
(local.set $expected ( f64.const 0x123456789ABCDEFabcdef.0 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( f64.const 0.0 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0x80 )))
(local.set $expected ( f64.const -0.0 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0x4000 )))
(local.set $expected ( f64.const 2.0 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0xc000 )))
(local.set $expected ( f64.const -2.0 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0xffffffff 0x7fefffff )))
(local.set $expected ( f64.const 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0x00100000 )))
(local.set $expected ( f64.const 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0xffffffff 0x000fffff )))
(local.set $expected ( f64.const 0x1.ffffffffffffep-1023 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane-last" (func $f (param v128) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( f64.const 0x0.0000000000002p-1023 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 127 )))
(local.set $expected ( v128.const i8x16 127 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 128 )))
(local.set $expected ( v128.const i8x16 -128 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 255 )))
(local.set $expected ( v128.const i8x16 -1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 256 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const -129 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 127 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 32767 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const -32768 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 32767 )))
(local.set $expected ( v128.const i16x8 32767 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 32768 )))
(local.set $expected ( v128.const i16x8 -32768 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 65535 )))
(local.set $expected ( v128.const i16x8 -1 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 65536 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 012345 )))
(local.set $expected ( v128.const i16x8 012_345 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const -0x01234 )))
(local.set $expected ( v128.const i16x8 -0x0_1234 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const -32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const -32769 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 0x7fffffff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 0x80000000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 054321 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 054_321 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const -0x04321 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 -0x0_4321 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const 2147483647 )))
(local.set $expected ( v128.const i32x4 2147483647 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const 4294967295 )))
(local.set $expected ( v128.const i32x4 -1 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const 01234567890 )))
(local.set $expected ( v128.const i32x4 01_234_567_890 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane-first" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const -0x012345678 )))
(local.set $expected ( v128.const i32x4 -0x0_1234_5678 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const 2147483648 )))
(local.set $expected ( v128.const i32x4 0 0 0 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const -2147483648 )))
(local.set $expected ( v128.const i32x4 0 0 0 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const 01234567890 )))
(local.set $expected ( v128.const i32x4 0 0 0 01_234_567_890 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane-last" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const -0x012345678 )))
(local.set $expected ( v128.const i32x4 0 0 0 -0x0_1234_5678 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 53.0 )))
(local.set $expected ( v128.const f32x4 53.0 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( f32.const 53.0 )))
(local.set $expected ( v128.const f32x4 53.0 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const nan )))
(local.set $expected ( v128.const f32x4 nan 0.0 0.0 0.0 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF  0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF  0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const inf )))
(local.set $expected ( v128.const f32x4 inf 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan 0.0 0.0 0.0 ) ( f32.const 3.14 )))
(local.set $expected ( v128.const f32x4 3.14 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf 0.0 0.0 0.0 ) ( f32.const 1e38 )))
(local.set $expected ( v128.const f32x4 1e38 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf 0.0 0.0 0.0 ) ( f32.const 0x1.fffffep127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffep127 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf 0.0 0.0 0.0 ) ( f32.const 0x1p127 )))
(local.set $expected ( v128.const f32x4 0x1p127 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 0123456789 )))
(local.set $expected ( v128.const f32x4 0123456789 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 0123456789. )))
(local.set $expected ( v128.const f32x4 0123456789. 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 0x0123456789ABCDEF )))
(local.set $expected ( v128.const f32x4 0x0123456789ABCDEF 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-first" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 0x0123456789ABCDEF. )))
(local.set $expected ( v128.const f32x4 0x0123456789ABCDEF. 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const -53.0 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 -53.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( f32.const -53.0 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 -53.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const nan )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0xFFFFFFFF  0xFFFFFFFF  0xFFFFFFFF  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0xFFFFFFFF  0xFFFFFFFF  0xFFFFFFFF  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const -inf )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 nan ) ( f32.const 3.14 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 3.14 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 -inf ) ( f32.const -1e38 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 -1e38 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 -inf ) ( f32.const -0x1.fffffep127 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 -0x1.fffffep127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 -inf ) ( f32.const -0x1p127 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 -0x1p127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 0x0123456789ABCDEFp019 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 0x0123456789ABCDEFp019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane-last" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ( f32.const 0x0123456789ABCDEF.p-019 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 0x0123456789ABCDEF.p-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane-first" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane-first" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 18446744073709551615 )))
(local.set $expected ( v128.const i64x2 -1 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane-first" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 01234567890123456789 )))
(local.set $expected ( v128.const i64x2 01_234_567_890_123_456_789 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane-first" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 0x01234567890abcdef )))
(local.set $expected ( v128.const i64x2 0x0_1234_5678_90AB_cdef 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane-last" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 9223372036854775808 )))
(local.set $expected ( v128.const i64x2 0 9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane-last" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 9223372036854775808 )))
(local.set $expected ( v128.const i64x2 0 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane-last" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 01234567890123456789 )))
(local.set $expected ( v128.const i64x2 0 01_234_567_890_123_456_789 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane-last" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 0x01234567890abcdef )))
(local.set $expected ( v128.const i64x2 0 0x0_1234_5678_90AB_cdef ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 1.0 1.0 ) ( f64.const 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0.0 1.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -1.0 -1.0 ) ( f64.const -0x0p-0 )))
(local.set $expected ( v128.const f64x2 -0.0 -1.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 1.25 )))
(local.set $expected ( v128.const f64x2 1.25 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const -1.25 )))
(local.set $expected ( v128.const f64x2 -1.25 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan 0.0 ) ( f64.const -1.7976931348623157e+308 )))
(local.set $expected ( v128.const f64x2 -1.7976931348623157e+308 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan 0.0 ) ( f64.const 1.7976931348623157e+308 )))
(local.set $expected ( v128.const f64x2 1.7976931348623157e+308 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf 0.0 ) ( f64.const -0x1.fffffffffffffp-1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp-1023 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf 0.0 ) ( f64.const 0x1.fffffffffffffp-1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp-1023 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const -nan )))
(local.set $expected ( v128.const f64x2 -nan 0.0 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const nan )))
(local.set $expected ( v128.const f64x2 nan 0.0 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const -inf )))
(local.set $expected ( v128.const f64x2 -inf 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const inf )))
(local.set $expected ( v128.const f64x2 inf 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 0123456789 )))
(local.set $expected ( v128.const f64x2 0123456789 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 0123456789. )))
(local.set $expected ( v128.const f64x2 0123456789. 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 0x0123456789ABCDEFabcdef )))
(local.set $expected ( v128.const f64x2 0x0123456789ABCDEFabcdef 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-first" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 0x0123456789ABCDEFabcdef. )))
(local.set $expected ( v128.const f64x2 0x0123456789ABCDEFabcdef. 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 2.0 2.0 ) ( f64.const 0.0 )))
(local.set $expected ( v128.const f64x2 2.0 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -2.0 -2.0 ) ( f64.const -0.0 )))
(local.set $expected ( v128.const f64x2 -2.0 -0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 2.25 )))
(local.set $expected ( v128.const f64x2 0.0 2.25 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const -2.25 )))
(local.set $expected ( v128.const f64x2 0.0 -2.25 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 -nan ) ( f64.const -1.7976931348623157e+308 )))
(local.set $expected ( v128.const f64x2 0.0 -1.7976931348623157e+308 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 nan ) ( f64.const 1.7976931348623157e+308 )))
(local.set $expected ( v128.const f64x2 0.0 1.7976931348623157e+308 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 -inf ) ( f64.const -0x1.fffffffffffffp-1023 )))
(local.set $expected ( v128.const f64x2 0.0 -0x1.fffffffffffffp-1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 inf ) ( f64.const 0x1.fffffffffffffp-1023 )))
(local.set $expected ( v128.const f64x2 0.0 0x1.fffffffffffffp-1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const -nan )))
(local.set $expected ( v128.const f64x2 0.0 -nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0xFFFFFFFF 0xFFFFFFFF  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0xFFFFFFFF 0xFFFFFFFF  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const nan )))
(local.set $expected ( v128.const f64x2 0.0 nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0xFFFFFFFF 0xFFFFFFFF  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0xFFFFFFFF 0xFFFFFFFF  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const -inf )))
(local.set $expected ( v128.const f64x2 0.0 -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const inf )))
(local.set $expected ( v128.const f64x2 0.0 inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 0123456789e019 )))
(local.set $expected ( v128.const f64x2 0.0 0123456789e019 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 0123456789e+019 )))
(local.set $expected ( v128.const f64x2 0.0 0123456789e+019 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 0123456789.e019 )))
(local.set $expected ( v128.const f64x2 0.0 0123456789.e019 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane-last" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0.0 0.0 ) ( f64.const 0123456789.e-019 )))
(local.set $expected ( v128.const f64x2 0.0 0123456789.e-019 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 )))
(local.set $expected ( v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 ) ( v128.const i8x16 -8 -7 -6 -5 -4 -3 -2 -1 16 17 18 19 20 21 22 23 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 ) ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 )))
(local.set $expected ( v128.const i8x16 115 114 113 112 111 110 109 108 107 106 105 104 103 102 101 100 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 ) ( v128.const i8x16 -1 1 -2 2 -3 3 -4 4 -5 5 -6 6 -7 7 -8 8 )))
(local.set $expected ( v128.const i8x16 0 101 0 102 0 103 0 104 0 105 0 106 0 107 0 108 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 100 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 ) ( v128.const i8x16 9 16 10 17 11 18 12 19 13 20 14 21 15 22 16 23 )))
(local.set $expected ( v128.const i8x16 109 0 110 0 111 0 112 0 113 0 114 0 115 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x64 0x65 0x66 0x67 0x68 0x69 0x6a 0x6b 0x6c 0x6d 0x6e 0x6f 0x70 0x71 0x72 0x73 ) ( v128.const i8x16 9 16 10 17 11 18 12 19 13 20 14 21 15 22 16 23 )))
(local.set $expected ( v128.const i8x16 0x6d 0 0x6e 0 0x6f 0 0x70 0 0x71 0 0x72 0 0x73 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x6465 0x6667 0x6869 0x6a6b 0x6c6d 0x6e6f 0x7071 0x7273 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 )))
(local.set $expected ( v128.const i16x8 0x6465 0x6667 0x6869 0x6a6b 0x6c6d 0x6e6f 0x7071 0x7273 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x64656667 0x68696a6b 0x6c6d6e6f 0x70717273 ) ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 )))
(local.set $expected ( v128.const i32x4 0x73727170 0x6f6e6d6c 0x6b6a6968 0x67666564 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan -nan inf -inf ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 )))
(local.set $expected ( v128.const i32x4 0x7fc00000 0xffc00000 0x7f800000 0xff800000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x67666564 0x6b6a6968 0x6f6e6d5c 0x73727170 ) ( v128.const f32x4 0.0 -0.0 inf -inf )))
(local.set $expected ( v128.const i32x4 0x64646464 0x00646464 0x00006464 0x00006464 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-1" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-2" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 )))
(local.set $expected ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-3" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 )))
(local.set $expected ( v128.const i8x16 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-4" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 )))
(local.set $expected ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-5" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-6" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 )))
(local.set $expected ( v128.const i8x16 -16 -16 -16 -16 -16 -16 -16 -16 -16 -16 -16 -16 -16 -16 -16 -16 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-7" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 -16 -16 -16 -16 -16 -16 -16 -16 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-1" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x64 0x65 0x66 0x67 0x68 0x69 0x6a 0x6b 0x6c 0x6d 0x6e 0x6f 0x70 0x71 0x72 0x73 ) ( v128.const i8x16 0xf0 0xf1 0xf2 0xf3 0xf4 0xf5 0xf6 0xf7 0xf8 0xf9 0xfa 0xfb 0xfc 0xfd 0xfe 0xff )))
(local.set $expected ( v128.const i8x16 0x64 0x65 0x66 0x67 0x68 0x69 0x6a 0x6b 0x6c 0x6d 0x6e 0x6f 0x70 0x71 0x72 0x73 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-1" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0b0a 0x0d0c 0x0f0e ) ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 )))
(local.set $expected ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0b0a 0x0d0c 0x0f0e ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-2" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i32x4 0xf3f2f1f0 0xf7f6f5f4 0xfbfaf9f8 0xfffefdfc )))
(local.set $expected ( v128.const i32x4 0xf3f2f1f0 0xf7f6f5f4 0xfbfaf9f8 0xfffefdfc ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-1" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x10203 0x4050607 0x8090a0b 0xc0d0e0f ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 )))
(local.set $expected ( v128.const i32x4 0x10203 0x4050607 0x8090a0b 0xc0d0e0f ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-1" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 1.0 nan inf -inf ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 )))
(local.set $expected ( v128.const i32x4 0x3f800000 0x7fc00000 0x7f800000 0xff800000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-1" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x10203 0x4050607 0x8090a0b 0xc0d0e0f ) ( v128.const f32x4 -0.0 nan inf -inf )))
(local.set $expected ( v128.const i32x4 0x10203 0x4050607 0x8090a0b 0xc0d0e0f ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_swizzle" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1_234_567_890 0x1234_5678 01_234_567_890 0x0_1234_5678 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 )))
(local.set $expected ( v128.const i32x4 0x4996_02d2 0x1234_5678 0x4996_02d2 0x1234_5678 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16_shuffle-1" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1_234_567_890_123_456_789_0 0x1234_5678_90AB_cdef ) ( v128.const i64x2 01_234_567_890_123_456_789_0 0x0_1234_5678_90AB_cdef )))
(local.set $expected ( v128.const i32x4 0xeb1f_0ad2 0xab54_a98c 0x90ab_cdef 0x1234_5678 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i8x16.extract_lane_s  -1 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i8x16.extract_lane_s 256 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i8x16.extract_lane_u  -1 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i8x16.extract_lane_u 256 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i16x8.extract_lane_s  -1 (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i16x8.extract_lane_s 256 (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i16x8.extract_lane_u  -1 (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i16x8.extract_lane_u 256 (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i32x4.extract_lane  -1 (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i32x4.extract_lane 256 (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result f32) (f32x4.extract_lane  -1 (v128.const f32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result f32) (f32x4.extract_lane 256 (v128.const f32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i8x16.replace_lane  -1 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i8x16.replace_lane 256 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.replace_lane  -1 (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.replace_lane 256 (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.replace_lane  -1 (v128.const i32x4 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.replace_lane 256 (v128.const i32x4 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (f32x4.replace_lane  -1 (v128.const f32x4 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (f32x4.replace_lane 256 (v128.const f32x4 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i64) (i64x2.extract_lane  -1 (v128.const i64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i64) (i64x2.extract_lane 256 (v128.const i64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result f64) (f64x2.extract_lane  -1 (v128.const f64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result f64) (f64x2.extract_lane 256 (v128.const f64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i64x2.replace_lane  -1 (v128.const i64x2 0 0) (i64.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i64x2.replace_lane 256 (v128.const i64x2 0 0) (i64.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (f64x2.replace_lane  -1 (v128.const f64x2 0 0) (f64.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (f64x2.replace_lane 256 (v128.const f64x2 0 0) (f64.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_s 16 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_s 255 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_u 16 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_u 255 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.extract_lane_s 8 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.extract_lane_s 255 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.extract_lane_u 8 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.extract_lane_u 255 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i32x4.extract_lane 4 ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i32x4.extract_lane 255 ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result f32 ) ( f32x4.extract_lane 4 ( v128.const f32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result f32 ) ( f32x4.extract_lane 255 ( v128.const f32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.replace_lane 16 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.replace_lane 255 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.replace_lane 16 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.replace_lane 255 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.replace_lane 4 ( v128.const i32x4 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.replace_lane 255 ( v128.const i32x4 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.replace_lane 4 ( v128.const f32x4 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.replace_lane 255 ( v128.const f32x4 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i64 ) ( i64x2.extract_lane 2 ( v128.const i64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i64 ) ( i64x2.extract_lane 255 ( v128.const i64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result f64 ) ( f64x2.extract_lane 2 ( v128.const f64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result f64 ) ( f64x2.extract_lane 255 ( v128.const f64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.replace_lane 2 ( v128.const i64x2 0 0 ) ( i64.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.replace_lane 255 ( v128.const i64x2 0 0 ) ( i64.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f64x2.replace_lane 2 ( v128.const f64x2 0 0 ) ( f64.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f64x2.replace_lane 255 ( v128.const f64x2 0 0 ) ( f64.const 1.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.extract_lane_s 8 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.extract_lane_u 8 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i32x4.extract_lane 4 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( f32x4.extract_lane 4 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.replace_lane 8 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.replace_lane 4 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.replace_lane 4 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i64 ) ( i64x2.extract_lane 2 ( v128.const i64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result f64 ) ( f64x2.extract_lane 2 ( v128.const f64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.replace_lane 2 ( v128.const i64x2 0 0 ) ( i64.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f64x2.replace_lane 2 ( v128.const f64x2 0 0 ) ( f64.const 1.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_s 0 ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_u 0 ( i64.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_s 0 ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_u 0 ( f64.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i32x4.extract_lane 0 ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result f32 ) ( f32x4.extract_lane 0 ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.replace_lane 0 ( i32.const 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.replace_lane 0 ( i64.const 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.replace_lane 0 ( i32.const 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.replace_lane 0 ( f32.const 0.0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i64 ) ( i64x2.extract_lane 0 ( i64.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result f64 ) ( f64x2.extract_lane 0 ( f64.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.replace_lane 0 ( i32.const 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.replace_lane 0 ( f32.const 0.0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.replace_lane 0 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( f32.const 1.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.replace_lane 0 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( f64.const 1.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.replace_lane 0 ( v128.const i32x4 0 0 0 0 ) ( f32.const 1.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.replace_lane 0 ( v128.const f32x4 0 0 0 0 ) ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.replace_lane 0 ( v128.const i64x2 0 0 ) ( f64.const 1.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f64x2.replace_lane 0 ( v128.const f64x2 0 0 ) ( i64.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( v8x16.swizzle ( i32.const 1 ) ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( v8x16.swizzle ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ) ( i32.const 2 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ( f32.const 3.0 ) ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ) ( f32.const 4.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param v128) (result v128)
local.get 0
local.get 0
v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param v128) (result v128)
local.get 0
local.get 0
v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128)
(v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 -1
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128)
(v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 256
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 255 ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i8x16.extract_lane 0 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i16x8.extract_lane 0 (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i32x4.extract_lane_s 0 (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i32x4.extract_lane_u 0 (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i64x2.extract_lane_s 0 (v128.const i64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i64x2.extract_lane_u 0 (v128.const i64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(v8x16.shuffle1 (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(v8x16.shuffle2_imm  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) 
(v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(i8x16.swizzle (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(i8x16.shuffle  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) 
(v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result i32) (i8x16.extract_lane_s (local.get 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result i32) (i8x16.extract_lane_u (local.get 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result i32) (i16x8.extract_lane_s (local.get 0) (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result i32) (i16x8.extract_lane_u (local.get 0) (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result i32) (i32x4.extract_lane (local.get 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result f32) (f32x4.extract_lane (local.get 0) (v128.const f32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result v128) (i8x16.replace_lane (local.get 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result v128) (i16x8.replace_lane (local.get 0) (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result v128) (i32x4.replace_lane (local.get 0) (v128.const i32x4 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result v128) (f32x4.replace_lane (local.get 0) (v128.const f32x4 0 0 0 0) (f32.const 1.0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param v128) (result v128) 
(v8x16.shuffle (local.get 0) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result i64) (i64x2.extract_lane (local.get 0) (v128.const i64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result f64) (f64x2.extract_lane (local.get 0) (v128.const f64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result v128) (i64x2.replace_lane (local.get 0) (v128.const i64x2 0 0) (i64.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (param i32) (result v128) (f64x2.replace_lane (local.get 0) (v128.const f64x2 0 0) (f64.const 1.0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i8x16.extract_lane_s 1.5 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i8x16.extract_lane_u nan (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i16x8.extract_lane_s inf (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i16x8.extract_lane_u -inf (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i32x4.extract_lane nan (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result f32) (f32x4.extract_lane nan (v128.const f32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i8x16.replace_lane -2.5 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.replace_lane nan (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.replace_lane inf (v128.const i32x4 0 0 0 0) (i32.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (f32x4.replace_lane -inf (v128.const f32x4 0 0 0 0) (f32.const 1.1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i64) (i64x2.extract_lane nan (v128.const i64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result f64) (f64x2.extract_lane nan (v128.const f64x2 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i64x2.replace_lane inf (v128.const i64x2 0 0) (i64.const 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (f64x2.replace_lane -inf (v128.const f64x2 0 0) (f64.const 1.1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(v8x16.shuffle (v128.const i8x16 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15.0) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(v8x16.shuffle 0.5 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(v8x16.shuffle -inf 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 inf) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) 
(v8x16.shuffle nan 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15) 
(v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0) 
(v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i8x16_extract_lane_s" ) ( param v128 v128 ) ( result v128 ) ( i8x16.replace_lane 0 ( local.get 0 ) ( i8x16.extract_lane_s 0 ( local.get 1 ) ) ) ) ( func ( export "i8x16_extract_lane_u" ) ( param v128 v128 ) ( result v128 ) ( i8x16.replace_lane 0 ( local.get 0 ) ( i8x16.extract_lane_u 0 ( local.get 1 ) ) ) ) ( func ( export "i16x8_extract_lane_s" ) ( param v128 v128 ) ( result v128 ) ( i16x8.replace_lane 0 ( local.get 0 ) ( i16x8.extract_lane_s 0 ( local.get 1 ) ) ) ) ( func ( export "i16x8_extract_lane_u" ) ( param v128 v128 ) ( result v128 ) ( i16x8.replace_lane 0 ( local.get 0 ) ( i16x8.extract_lane_u 0 ( local.get 1 ) ) ) ) ( func ( export "i32x4_extract_lane" ) ( param v128 v128 ) ( result v128 ) ( i32x4.replace_lane 0 ( local.get 0 ) ( i32x4.extract_lane 0 ( local.get 1 ) ) ) ) ( func ( export "f32x4_extract_lane" ) ( param v128 v128 ) ( result v128 ) ( i32x4.replace_lane 0 ( local.get 0 ) ( i32x4.extract_lane 0 ( local.get 1 ) ) ) ) ( func ( export "i64x2_extract_lane" ) ( param v128 v128 ) ( result v128 ) ( i64x2.replace_lane 0 ( local.get 0 ) ( i64x2.extract_lane 0 ( local.get 1 ) ) ) ) ( func ( export "f64x2_extract_lane" ) ( param v128 v128 ) ( result v128 ) ( f64x2.replace_lane 0 ( local.get 0 ) ( f64x2.extract_lane 0 ( local.get 1 ) ) ) ) ( func ( export "i8x16_replace_lane-s" ) ( param v128 i32 ) ( result i32 ) ( i8x16.extract_lane_s 15 ( i8x16.replace_lane 15 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i8x16_replace_lane-u" ) ( param v128 i32 ) ( result i32 ) ( i8x16.extract_lane_u 15 ( i8x16.replace_lane 15 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_replace_lane-s" ) ( param v128 i32 ) ( result i32 ) ( i16x8.extract_lane_s 7 ( i16x8.replace_lane 7 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_replace_lane-u" ) ( param v128 i32 ) ( result i32 ) ( i16x8.extract_lane_u 7 ( i16x8.replace_lane 7 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_replace_lane" ) ( param v128 i32 ) ( result i32 ) ( i32x4.extract_lane 3 ( i32x4.replace_lane 3 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "f32x4_replace_lane" ) ( param v128 f32 ) ( result f32 ) ( f32x4.extract_lane 3 ( f32x4.replace_lane 3 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i64x2_replace_lane" ) ( param v128 i64 ) ( result i64 ) ( i64x2.extract_lane 1 ( i64x2.replace_lane 1 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "f64x2_replace_lane" ) ( param v128 f64 ) ( result f64 ) ( f64x2.extract_lane 1 ( f64x2.replace_lane 1 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "as-v8x16_swizzle-operand" ) ( param v128 i32 v128 ) ( result v128 ) ( v8x16.swizzle ( i8x16.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "as-v8x16_shuffle-operands" ) ( param v128 i32 v128 i32 ) ( result v128 ) ( v8x16.shuffle 16 1 18 3 20 5 22 7 24 9 26 11 28 13 30 15 ( i8x16.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ( i8x16.replace_lane 15 ( local.get 2 ) ( local.get 3 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_extract_lane_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 255 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_extract_lane_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 65535 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_extract_lane" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0x10000 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 65536 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_extract_lane" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 0 0 0 ) ( v128.const f32x4 1e38 nan nan nan )))
(local.set $expected ( v128.const f32x4 1e38 0 0 0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-s" (func $f (param v128 i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 255 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_replace_lane-u" (func $f (param v128 i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 255 )))
(local.set $expected ( i32.const 255 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-s" (func $f (param v128 i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 65535 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_replace_lane-u" (func $f (param v128 i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 65535 )))
(local.set $expected ( i32.const 65535 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_replace_lane" (func $f (param v128 i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const -1 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_replace_lane" (func $f (param v128 f32) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f32x4 0 0 0 0 ) ( f32.const 1.25 )))
(local.set $expected ( f32.const 1.25 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_extract_lane" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 0xffffffffffffffff -1 )))
(local.set $expected ( v128.const i64x2 0xffffffffffffffff 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_extract_lane" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 0 ) ( v128.const f64x2 1e308 nan )))
(local.set $expected ( v128.const f64x2 1e308 0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2_replace_lane" (func $f (param v128 i64) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const -1 )))
(local.set $expected ( i64.const -1 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2_replace_lane" (func $f (param v128 f64) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const f64x2 0 0 ) ( f64.const 2.5 )))
(local.set $expected ( f64.const 2.5 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v8x16_swizzle-operand" (func $f (param v128 i32 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( i32.const 255 ) ( v128.const i8x16 -1 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 )))
(local.set $expected ( v128.const i8x16 0 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v8x16_shuffle-operands" (func $f (param v128 i32 v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 255 0 255 15 255 0 255 255 255 0 255 127 255 0 255 ) ( i32.const 1 ) ( v128.const i8x16 0x55 0 0x55 0 0x55 0 0x55 0 0x55 0 0x55 0 0x55 1 0x55 -1 ) ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 0x55 0xff 0x55 0xff 0x55 0xff 0x55 0xff 0x55 0xff 0x55 0xff 0x55 0xff 0x55 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "as-i8x16_splat-operand" ) ( param v128 ) ( result v128 ) ( i8x16.splat ( i8x16.extract_lane_s 0 ( local.get 0 ) ) ) ) ( func ( export "as-i16x8_splat-operand" ) ( param v128 ) ( result v128 ) ( i16x8.splat ( i16x8.extract_lane_u 0 ( local.get 0 ) ) ) ) ( func ( export "as-i32x4_splat-operand" ) ( param v128 ) ( result v128 ) ( i32x4.splat ( i32x4.extract_lane 0 ( local.get 0 ) ) ) ) ( func ( export "as-f32x4_splat-operand" ) ( param v128 ) ( result v128 ) ( f32x4.splat ( f32x4.extract_lane 0 ( local.get 0 ) ) ) ) ( func ( export "as-i64x2_splat-operand" ) ( param v128 ) ( result v128 ) ( i64x2.splat ( i64x2.extract_lane 0 ( local.get 0 ) ) ) ) ( func ( export "as-f64x2_splat-operand" ) ( param v128 ) ( result v128 ) ( f64x2.splat ( f64x2.extract_lane 0 ( local.get 0 ) ) ) ) ( func ( export "as-i8x16_add-operands" ) ( param v128 i32 v128 i32 ) ( result v128 ) ( i8x16.add ( i8x16.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ( i8x16.replace_lane 15 ( local.get 2 ) ( local.get 3 ) ) ) ) ( func ( export "as-i16x8_add-operands" ) ( param v128 i32 v128 i32 ) ( result v128 ) ( i16x8.add ( i16x8.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ( i16x8.replace_lane 7 ( local.get 2 ) ( local.get 3 ) ) ) ) ( func ( export "as-i32x4_add-operands" ) ( param v128 i32 v128 i32 ) ( result v128 ) ( i32x4.add ( i32x4.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ( i32x4.replace_lane 3 ( local.get 2 ) ( local.get 3 ) ) ) ) ( func ( export "as-i64x2_add-operands" ) ( param v128 i64 v128 i64 ) ( result v128 ) ( i64x2.add ( i64x2.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ( i64x2.replace_lane 1 ( local.get 2 ) ( local.get 3 ) ) ) ) ( func ( export "swizzle-as-i8x16_add-operands" ) ( param v128 v128 v128 v128 ) ( result v128 ) ( i8x16.add ( v8x16.swizzle ( local.get 0 ) ( local.get 1 ) ) ( v8x16.swizzle ( local.get 2 ) ( local.get 3 ) ) ) ) ( func ( export "shuffle-as-i8x16_sub-operands" ) ( param v128 v128 v128 v128 ) ( result v128 ) ( i8x16.sub ( v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ( local.get 0 ) ( local.get 1 ) ) ( v8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 ( local.get 2 ) ( local.get 3 ) ) ) ) ( func ( export "as-i8x16_any_true-operand" ) ( param v128 i32 ) ( result i32 ) ( i8x16.any_true ( i8x16.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "as-i16x8_any_true-operand" ) ( param v128 i32 ) ( result i32 ) ( i16x8.any_true ( i16x8.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "as-i32x4_any_true-operand1" ) ( param v128 i32 ) ( result i32 ) ( i32x4.any_true ( i32x4.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "as-i32x4_any_true-operand2" ) ( param v128 i64 ) ( result i32 ) ( i32x4.any_true ( i64x2.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "swizzle-as-i8x16_all_true-operands" ) ( param v128 v128 ) ( result i32 ) ( i8x16.all_true ( v8x16.swizzle ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "shuffle-as-i8x16_any_true-operands" ) ( param v128 v128 ) ( result i32 ) ( i8x16.any_true ( v8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ( local.get 0 ) ( local.get 1 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_splat-operand" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xff 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_splat-operand" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_splat-operand" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x10000 0 0 0 )))
(local.set $expected ( v128.const i32x4 65536 65536 65536 65536 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f32x4_splat-operand" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 3.14 nan nan nan )))
(local.set $expected ( v128.const f32x4 3.14 3.14 3.14 3.14 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i64x2_splat-operand" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -1 0 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f64x2_splat-operand" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf nan )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_add-operands" (func $f (param v128 i32 v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xff 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 ) ( i32.const 1 ) ( v128.const i8x16 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 0xff ) ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_add-operands" (func $f (param v128 i32 v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 4 9 16 25 36 49 64 ) ( i32.const 1 ) ( v128.const i16x8 64 49 36 25 16 9 4 -1 ) ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 65 53 45 41 41 45 53 65 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_add-operands" (func $f (param v128 i32 v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 8 27 64 ) ( i32.const 1 ) ( v128.const i32x4 64 27 8 -1 ) ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 65 35 35 65 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i64x2_add-operands" (func $f (param v128 i64 v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -1 8 ) ( i64.const 1 ) ( v128.const i64x2 64 27 ) ( i64.const 1 )))
(local.set $expected ( v128.const i64x2 65 9 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "swizzle-as-i8x16_add-operands" (func $f (param v128 v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "shuffle-as-i8x16_sub-operands" (func $f (param v128 v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ) ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 ) ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 )))
(local.set $expected ( v128.const i8x16 -15 -13 -11 -9 -7 -5 -3 -1 1 3 5 7 9 11 13 15 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_any_true-operand" (func $f (param v128 i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_any_true-operand" (func $f (param v128 i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_any_true-operand1" (func $f (param v128 i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 0 0 0 ) ( i32.const 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_any_true-operand2" (func $f (param v128 i64) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 1 0 ) ( i64.const 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "swizzle-as-i8x16_all_true-operands" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "swizzle-as-i8x16_all_true-operands" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 16 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "shuffle-as-i8x16_any_true-operands" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "as-v128_store-operand-1" ) ( param v128 i32 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( i8x16.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-2" ) ( param v128 i32 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( i16x8.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-3" ) ( param v128 i32 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( i32x4.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-4" ) ( param v128 f32 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( f32x4.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-5" ) ( param v128 i64 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( i64x2.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-6" ) ( param v128 f64 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( f64x2.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( v128.load ( i32.const 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-1" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-2" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 256 )))
(local.set $expected ( v128.const i16x8 0x100 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-3" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( i32.const 0xffffffff )))
(local.set $expected ( v128.const i32x4 -1 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-4" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 0 0 0 ) ( f32.const 3.14 )))
(local.set $expected ( v128.const f32x4 3.14 0 0 0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-5" (func $f (param v128 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( i64.const 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 -1 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-6" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 0 ) ( f64.const 3.14 )))
(local.set $expected ( v128.const f64x2 3.14 0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( global $g ( mut v128 ) ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ) ( global $h ( mut v128 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ( func ( export "as-if-condition-value" ) ( param v128 ) ( result i32 ) ( if ( result i32 ) ( i8x16.extract_lane_s 0 ( local.get 0 ) ) ( then ( i32.const 0xff ) ) ( else ( i32.const 0 ) ) ) ) ( func ( export "as-return-value-1" ) ( param v128 i32 ) ( result v128 ) ( return ( i16x8.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "as-local_set-value" ) ( param v128 ) ( result i32 ) ( local i32 ) ( local.set 1 ( i32x4.extract_lane 0 ( local.get 0 ) ) ) ( return ( local.get 1 ) ) ) ( func ( export "as-global_set-value-1" ) ( param v128 f32 ) ( result v128 ) ( global.set $g ( f32x4.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( return ( global.get $g ) ) ) ( func ( export "as-return-value-2" ) ( param v128 v128 ) ( result v128 ) ( return ( v8x16.swizzle ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "as-global_set-value-2" ) ( param v128 v128 ) ( result v128 ) ( global.set $h ( v8x16.shuffle 0 1 2 3 4 5 6 7 24 25 26 27 28 29 30 31 ( local.get 0 ) ( local.get 1 ) ) ) ( return ( global.get $h ) ) ) ( func ( export "as-local_set-value-1" ) ( param v128 ) ( result i64 ) ( local i64 ) ( local.set 1 ( i64x2.extract_lane 0 ( local.get 0 ) ) ) ( return ( local.get 1 ) ) ) ( func ( export "as-global_set-value-3" ) ( param v128 f64 ) ( result v128 ) ( global.set $g ( f64x2.replace_lane 0 ( local.get 0 ) ( local.get 1 ) ) ) ( return ( global.get $g ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-if-condition-value" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-return-value-1" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 1 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-local_set-value" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const -1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-global_set-value-1" (func $f (param v128 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 0 0 0 ) ( f32.const 3.14 )))
(local.set $expected ( v128.const f32x4 3.14 0 0 0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-return-value-2" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 ) ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 )))
(local.set $expected ( v128.const i8x16 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-global_set-value-2" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 -8 -7 -6 -5 -4 -3 -2 -1 ) ( v128.const i8x16 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 )))
(local.set $expected ( v128.const i8x16 -16 -15 -14 -13 -12 -11 -10 -9 8 7 6 5 4 3 2 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-local_set-value-1" (func $f (param v128) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i64x2 -1 -1 )))
(local.set $expected ( i64.const -1 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-global_set-value-3" (func $f (param v128 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 0 ) ( f64.const 3.14 )))
(local.set $expected ( v128.const f64x2 3.14 0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_s 0x0f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.extract_lane_u +0x0f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.extract_lane_s 0x07 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.extract_lane_u 0x0_7 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result i32 ) ( i32x4.extract_lane 03 ( v128.const i32x4 0 0 0 0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result f32 ) ( f32x4.extract_lane +03 ( v128.const f32x4 0 0 0 0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result i64 ) ( i64x2.extract_lane +1 ( v128.const i64x2 0 0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result f64 ) ( f64x2.extract_lane 0x1 ( v128.const f64x2 0 0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.replace_lane +015 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.replace_lane +0x7 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( i32.const 1 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.replace_lane +3 ( v128.const i32x4 0 0 0 0 ) ( i32.const 1 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.replace_lane 0x3 ( v128.const f32x4 0 0 0 0 ) ( f32.const 1.0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.replace_lane 01 ( v128.const i64x2 0 0 ) ( i64.const 1 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result v128 ) ( f64x2.replace_lane +0x01 ( v128.const f64x2 0 0 ) ( f64.const 1.0 ) ) ) )
`)));
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( result v128 ) ( v8x16.shuffle 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x18 0x19 0x1a 0x1b 0x1c 0x1d 0x1e 0x1f ( v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ) ) )
`)));
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result i32) (i8x16.extract_lane_s 1.0 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i8x16.extract_lane_s-1st-arg-empty (result i32)
  (i8x16.extract_lane_s (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.extract_lane_s-2nd-arg-empty ( result i32 ) ( i8x16.extract_lane_s 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i8x16.extract_lane_s-arg-empty (result i32)
  (i8x16.extract_lane_s)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i16x8.extract_lane_u-1st-arg-empty (result i32)
  (i16x8.extract_lane_u (v128.const i16x8 0 0 0 0 0 0 0 0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.extract_lane_u-2nd-arg-empty ( result i32 ) ( i16x8.extract_lane_u 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i16x8.extract_lane_u-arg-empty (result i32)
  (i16x8.extract_lane_u)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i32x4.extract_lane-1st-arg-empty (result i32)
  (i32x4.extract_lane (v128.const i32x4 0 0 0 0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.extract_lane-2nd-arg-empty ( result i32 ) ( i32x4.extract_lane 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i32x4.extract_lane-arg-empty (result i32)
  (i32x4.extract_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i64x2.extract_lane-1st-arg-empty (result i64)
  (i64x2.extract_lane (v128.const i64x2 0 0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.extract_lane-2nd-arg-empty ( result i64 ) ( i64x2.extract_lane 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i64x2.extract_lane-arg-empty (result i64)
  (i64x2.extract_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $f32x4.extract_lane-1st-arg-empty (result f32)
  (f32x4.extract_lane (v128.const f32x4 0 0 0 0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.extract_lane-2nd-arg-empty ( result f32 ) ( f32x4.extract_lane 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $f32x4.extract_lane-arg-empty (result f32)
  (f32x4.extract_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $f64x2.extract_lane-1st-arg-empty (result f64)
  (f64x2.extract_lane (v128.const f64x2 0 0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.extract_lane-2nd-arg-empty ( result f64 ) ( f64x2.extract_lane 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $f64x2.extract_lane-arg-empty (result f64)
  (f64x2.extract_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i8x16.replace_lane-1st-arg-empty (result v128)
  (i8x16.replace_lane (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (i32.const 1))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.replace_lane-2nd-arg-empty ( result v128 ) ( i8x16.replace_lane 0 ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.replace_lane-3rd-arg-empty ( result v128 ) ( i8x16.replace_lane 0 ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i8x16.replace_lane-arg-empty (result v128)
  (i8x16.replace_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i16x8.replace_lane-1st-arg-empty (result v128)
  (i16x8.replace_lane (v128.const i16x8 0 0 0 0 0 0 0 0) (i32.const 1))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.replace_lane-2nd-arg-empty ( result v128 ) ( i16x8.replace_lane 0 ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.replace_lane-3rd-arg-empty ( result v128 ) ( i16x8.replace_lane 0 ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i16x8.replace_lane-arg-empty (result v128)
  (i16x8.replace_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i32x4.replace_lane-1st-arg-empty (result v128)
  (i32x4.replace_lane (v128.const i32x4 0 0 0 0) (i32.const 1))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.replace_lane-2nd-arg-empty ( result v128 ) ( i32x4.replace_lane 0 ( i32.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.replace_lane-3rd-arg-empty ( result v128 ) ( i32x4.replace_lane 0 ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i32x4.replace_lane-arg-empty (result v128)
  (i32x4.replace_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $f32x4.replace_lane-1st-arg-empty (result v128)
  (f32x4.replace_lane (v128.const f32x4 0 0 0 0) (f32.const 1.0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.replace_lane-2nd-arg-empty ( result v128 ) ( f32x4.replace_lane 0 ( f32.const 1.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.replace_lane-3rd-arg-empty ( result v128 ) ( f32x4.replace_lane 0 ( v128.const f32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $f32x4.replace_lane-arg-empty (result v128)
  (f32x4.replace_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i64x2.replace_lane-1st-arg-empty (result v128)
  (i64x2.replace_lane (v128.const i64x2 0 0) (i64.const 1))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.replace_lane-2nd-arg-empty ( result v128 ) ( i64x2.replace_lane 0 ( i64.const 1 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.replace_lane-3rd-arg-empty ( result v128 ) ( i64x2.replace_lane 0 ( v128.const i64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $i64x2.replace_lane-arg-empty (result v128)
  (i64x2.replace_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $f64x2.replace_lane-1st-arg-empty (result v128)
  (f64x2.replace_lane (v128.const f64x2 0 0) (f64.const 1.0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.replace_lane-2nd-arg-empty ( result v128 ) ( f64x2.replace_lane 0 ( f64.const 1.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.replace_lane-3rd-arg-empty ( result v128 ) ( f64x2.replace_lane 0 ( v128.const f64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $f64x2.replace_lane-arg-empty (result v128)
  (f64x2.replace_lane)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $v8x16.shuffle-1st-arg-empty (result v128)
  (v8x16.shuffle
    (v128.const i8x16 0 1 2 3 5 6 6 7 8 9 10 11 12 13 14 15)
    (v128.const i8x16 1 2 3 5 6 6 7 8 9 10 11 12 13 14 15 16)
  )
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $v8x16.shuffle-2nd-arg-empty ( result v128 ) ( v8x16.shuffle 0 1 2 3 5 6 6 7 8 9 10 11 12 13 14 15 ( v128.const i8x16 1 2 3 5 6 6 7 8 9 10 11 12 13 14 15 16 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func $v8x16.shuffle-arg-empty (result v128)
  (v8x16.shuffle)
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)

