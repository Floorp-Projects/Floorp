var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i32x4.trunc_sat_f32x4_s" ) ( param v128 ) ( result v128 ) ( i32x4.trunc_sat_f32x4_s ( local.get 0 ) ) ) ( func ( export "i32x4.trunc_sat_f32x4_u" ) ( param v128 ) ( result v128 ) ( i32x4.trunc_sat_f32x4_u ( local.get 0 ) ) ) ( func ( export "f32x4.convert_i32x4_s" ) ( param v128 ) ( result v128 ) ( f32x4.convert_i32x4_s ( local.get 0 ) ) ) ( func ( export "f32x4.convert_i32x4_u" ) ( param v128 ) ( result v128 ) ( f32x4.convert_i32x4_u ( local.get 0 ) ) ) ( func ( export "i8x16.narrow_i16x8_s" ) ( param v128 v128 ) ( result v128 ) ( i8x16.narrow_i16x8_s ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i8x16.narrow_i16x8_u" ) ( param v128 v128 ) ( result v128 ) ( i8x16.narrow_i16x8_u ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i16x8.narrow_i32x4_s" ) ( param v128 v128 ) ( result v128 ) ( i16x8.narrow_i32x4_s ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i16x8.narrow_i32x4_u" ) ( param v128 v128 ) ( result v128 ) ( i16x8.narrow_i32x4_u ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i16x8.widen_high_i8x16_s" ) ( param v128 ) ( result v128 ) ( i16x8.widen_high_i8x16_s ( local.get 0 ) ) ) ( func ( export "i16x8.widen_high_i8x16_u" ) ( param v128 ) ( result v128 ) ( i16x8.widen_high_i8x16_u ( local.get 0 ) ) ) ( func ( export "i16x8.widen_low_i8x16_s" ) ( param v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_s ( local.get 0 ) ) ) ( func ( export "i16x8.widen_low_i8x16_u" ) ( param v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_u ( local.get 0 ) ) ) ( func ( export "i32x4.widen_high_i16x8_s" ) ( param v128 ) ( result v128 ) ( i32x4.widen_high_i16x8_s ( local.get 0 ) ) ) ( func ( export "i32x4.widen_high_i16x8_u" ) ( param v128 ) ( result v128 ) ( i32x4.widen_high_i16x8_u ( local.get 0 ) ) ) ( func ( export "i32x4.widen_low_i16x8_s" ) ( param v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_s ( local.get 0 ) ) ) ( func ( export "i32x4.widen_low_i16x8_u" ) ( param v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_u ( local.get 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0.0 -0.0 -0.0 -0.0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 1.5 1.5 1.5 1.5 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -1.5 -1.5 -1.5 -1.5 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 1.9 1.9 1.9 1.9 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 2.0 2.0 2.0 2.0 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -1.9 -1.9 -1.9 -1.9 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -2.0 -2.0 -2.0 -2.0 )))
(local.set $expected ( v128.const i32x4 -2 -2 -2 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 2147483520.0 2147483520.0 2147483520.0 2147483520.0 )))
(local.set $expected ( v128.const i32x4 2147483520 2147483520 2147483520 2147483520 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -2147483520.0 -2147483520.0 -2147483520.0 -2147483520.0 )))
(local.set $expected ( v128.const i32x4 -2147483520 -2147483520 -2147483520 -2147483520 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 2147483648.0 2147483648.0 2147483648.0 2147483648.0 )))
(local.set $expected ( v128.const i32x4 2147483647 2147483647 2147483647 2147483647 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -2147483648.0 -2147483648.0 -2147483648.0 -2147483648.0 )))
(local.set $expected ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 3000000000.0 3000000000.0 3000000000.0 3000000000.0 )))
(local.set $expected ( v128.const i32x4 2147483647 2147483647 2147483647 2147483647 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -3000000000.0 -3000000000.0 -3000000000.0 -3000000000.0 )))
(local.set $expected ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 2147483647.0 2147483647.0 2147483647.0 2147483647.0 )))
(local.set $expected ( v128.const i32x4 2147483647 2147483647 2147483647 2147483647 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -2147483647.0 -2147483647.0 -2147483647.0 -2147483647.0 )))
(local.set $expected ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.19999ap+0 0x1.19999ap+0 0x1.19999ap+0 0x1.19999ap+0 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.19999ap+0 -0x1.19999ap+0 -0x1.19999ap+0 -0x1.19999ap+0 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const i32x4 6 6 6 6 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const i32x4 -6 -6 -6 -6 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const i32x4 0x7fffffff 0x7fffffff 0x7fffffff 0x7fffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 +inf +inf +inf +inf )))
(local.set $expected ( v128.const i32x4 2147483647 2147483647 2147483647 2147483647 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 +nan +nan +nan +nan )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 42 nan inf -inf )))
(local.set $expected ( v128.const i32x4 42 0 2147483647 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -42 3.14 nan inf )))
(local.set $expected ( v128.const i32x4 -42 3 0 2147483647 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456792.0 0123456792.0 0123456792.0 0123456792.0 )))
(local.set $expected ( v128.const i32x4 123456792 123456792 123456792 123456792 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 01234567890.0 01234567890.0 01234567890.0 01234567890.0 )))
(local.set $expected ( v128.const i32x4 0x49960300 0x49960300 0x49960300 0x49960300 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0.0 0.0 0.0 0.0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0.0 -0.0 -0.0 -0.0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 1.5 1.5 1.5 1.5 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -1.5 -1.5 -1.5 -1.5 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 1.9 1.9 1.9 1.9 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 2.0 2.0 2.0 2.0 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -1.9 -1.9 -1.9 -1.9 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -2.0 -2.0 -2.0 -2.0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 2147483648.0 2147483648.0 2147483648.0 2147483648.0 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -2147483648.0 -2147483648.0 -2147483648.0 -2147483648.0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 3000000000.0 3000000000.0 3000000000.0 3000000000.0 )))
(local.set $expected ( v128.const i32x4 3000000000 3000000000 3000000000 3000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -3000000000.0 -3000000000.0 -3000000000.0 -3000000000.0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 2147483647.0 2147483647.0 2147483647.0 2147483647.0 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 4294967295.0 4294967295.0 4294967295.0 4294967295.0 )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 4294967296.0 4294967296.0 4294967296.0 4294967296.0 )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 4294967040.0 4294967040.0 4294967040.0 4294967040.0 )))
(local.set $expected ( v128.const i32x4 -256 -256 -256 -256 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.19999ap+0 0x1.19999ap+0 0x1.19999ap+0 0x1.19999ap+0 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.19999ap+0 -0x1.19999ap+0 -0x1.19999ap+0 -0x1.19999ap+0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.ccccccp-1 0x1.ccccccp-1 0x1.ccccccp-1 0x1.ccccccp-1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.ccccccp-1 -0x1.ccccccp-1 -0x1.ccccccp-1 -0x1.ccccccp-1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep-1 0x1.fffffep-1 0x1.fffffep-1 0x1.fffffep-1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep-1 -0x1.fffffep-1 -0x1.fffffep-1 -0x1.fffffep-1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const i32x4 6 6 6 6 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 +nan +nan +nan +nan )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 +inf +inf +inf +inf )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 42 nan inf -inf )))
(local.set $expected ( v128.const i32x4 42 0 0xffffffff 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -42 3.14 nan inf )))
(local.set $expected ( v128.const i32x4 0 3 0 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456792.0 0123456792.0 0123456792.0 0123456792.0 )))
(local.set $expected ( v128.const i32x4 123456792 123456792 123456792 123456792 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.trunc_sat_f32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0 -0123456789.0 -0123456789.0 -0123456789.0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const f32x4 1.0 1.0 1.0 1.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const f32x4 -1.0 -1.0 -1.0 -1.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 2147483647 2147483647 2147483647 2147483647 )))
(local.set $expected ( v128.const f32x4 2147483647.0 2147483647.0 2147483647.0 2147483647.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const f32x4 -2147483648.0 -2147483648.0 -2147483648.0 -2147483648.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1234567890 1234567890 1234567890 1234567890 )))
(local.set $expected ( v128.const f32x4 0x1.26580cp+30 0x1.26580cp+30 0x1.26580cp+30 0x1.26580cp+30 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0_123_456_792 0_123_456_792 0_123_456_792 0_123_456_792 )))
(local.set $expected ( v128.const f32x4 123456792.0 123456792.0 123456792.0 123456792.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x0_1234_5680 0x0_1234_5680 0x0_1234_5680 0x0_1234_5680 )))
(local.set $expected ( v128.const f32x4 305419904.0 305419904.0 305419904.0 305419904.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 16777217 16777217 16777217 16777217 )))
(local.set $expected ( v128.const f32x4 16777216.0 16777216.0 16777216.0 16777216.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -16777217 -16777217 -16777217 -16777217 )))
(local.set $expected ( v128.const f32x4 -16777216.0 -16777216.0 -16777216.0 -16777216.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 16777219 16777219 16777219 16777219 )))
(local.set $expected ( v128.const f32x4 16777220.0 16777220.0 16777220.0 16777220.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -16777219 -16777219 -16777219 -16777219 )))
(local.set $expected ( v128.const f32x4 -16777220.0 -16777220.0 -16777220.0 -16777220.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 -1 0x7fffffff 0x80000000 )))
(local.set $expected ( v128.const f32x4 0.0 -1.0 2147483647.0 -2147483648.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const f32x4 1.0 1.0 1.0 1.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const f32x4 4294967295.0 4294967295.0 4294967295.0 4294967295.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 2147483647 2147483647 2147483647 2147483647 )))
(local.set $expected ( v128.const f32x4 2147483648.0 2147483648.0 2147483648.0 2147483648.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const f32x4 2147483648.0 2147483648.0 2147483648.0 2147483648.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x12345678 0x12345678 0x12345678 0x12345678 )))
(local.set $expected ( v128.const f32x4 0x1.234568p+28 0x1.234568p+28 0x1.234568p+28 0x1.234568p+28 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80000080 0x80000080 0x80000080 0x80000080 )))
(local.set $expected ( v128.const f32x4 0x1.000000p+31 0x1.000000p+31 0x1.000000p+31 0x1.000000p+31 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80000081 0x80000081 0x80000081 0x80000081 )))
(local.set $expected ( v128.const f32x4 0x1.000002p+31 0x1.000002p+31 0x1.000002p+31 0x1.000002p+31 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80000082 0x80000082 0x80000082 0x80000082 )))
(local.set $expected ( v128.const f32x4 0x1.000002p+31 0x1.000002p+31 0x1.000002p+31 0x1.000002p+31 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xfffffe80 0xfffffe80 0xfffffe80 0xfffffe80 )))
(local.set $expected ( v128.const f32x4 0x1.fffffcp+31 0x1.fffffcp+31 0x1.fffffcp+31 0x1.fffffcp+31 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xfffffe81 0xfffffe81 0xfffffe81 0xfffffe81 )))
(local.set $expected ( v128.const f32x4 0x1.fffffep+31 0x1.fffffep+31 0x1.fffffep+31 0x1.fffffep+31 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xfffffe82 0xfffffe82 0xfffffe82 0xfffffe82 )))
(local.set $expected ( v128.const f32x4 0x1.fffffep+31 0x1.fffffep+31 0x1.fffffep+31 0x1.fffffep+31 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0_123_456_792 0_123_456_792 0_123_456_792 0_123_456_792 )))
(local.set $expected ( v128.const f32x4 123456792.0 123456792.0 123456792.0 123456792.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef )))
(local.set $expected ( v128.const f32x4 2427178496.0 2427178496.0 2427178496.0 2427178496.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 16777217 16777217 16777217 16777217 )))
(local.set $expected ( v128.const f32x4 16777216.0 16777216.0 16777216.0 16777216.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 16777219 16777219 16777219 16777219 )))
(local.set $expected ( v128.const f32x4 16777220.0 16777220.0 16777220.0 16777220.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.convert_i32x4_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 -1 0x7fffffff 0x80000000 )))
(local.set $expected ( v128.const f32x4 0.0 4294967295.0 2147483647.0 2147483648.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ) ( v128.const i16x8 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ) ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ) ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f ) ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i8x16 0x81 0x81 0x81 0x81 0x81 0x81 0x81 0x81 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 ) ( v128.const i16x8 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x81 0x81 0x81 0x81 0x81 0x81 0x81 0x81 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 ) ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 ) ( v128.const i16x8 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 ) ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 ) ( v128.const i16x8 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 ) ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 ) ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 -0x81 ) ( v128.const i16x8 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 ) ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ) ( v128.const i16x8 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ) ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe ) ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ) ( v128.const i16x8 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe )))
(local.set $expected ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe 0xfe ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ) ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 ) ( v128.const i16x8 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 )))
(local.set $expected ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ) ( v128.const i16x8 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 )))
(local.set $expected ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 ) ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 ) ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 ) ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.narrow_i16x8_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB ) ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x7ffe 0x7ffe 0x7ffe 0x7ffe ) ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 0x7ffe 0x7ffe 0x7ffe 0x7ffe 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i32x4 0x7ffe 0x7ffe 0x7ffe 0x7ffe )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7ffe 0x7ffe 0x7ffe 0x7ffe ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ) ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x7fff -0x7fff -0x7fff -0x7fff ) ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i16x8 0x8001 0x8001 0x8001 0x8001 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 ) ( v128.const i32x4 -0x7fff -0x7fff -0x7fff -0x7fff )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8001 0x8001 0x8001 0x8001 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 ) ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8001 -0x8001 -0x8001 -0x8001 ) ( v128.const i32x4 -0x8001 -0x8001 -0x8001 -0x8001 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8001 -0x8001 -0x8001 -0x8001 ) ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 ) ( v128.const i32x4 -0x8001 -0x8001 -0x8001 -0x8001 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 ) ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 ) ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8001 -0x8001 -0x8001 -0x8001 ) ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8000 -0x8000 -0x8000 -0x8000 ) ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8001 -0x8001 -0x8001 -0x8001 ) ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x8000000 -0x8000000 -0x8000000 -0x8000000 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0_123_456_789 0_123_456_789 0_123_456_789 0_123_456_789 ) ( v128.const i32x4 01_234_567_890 01_234_567_890 01_234_567_890 01_234_567_890 )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef ) ( v128.const i32x4 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xfffe 0xfffe 0xfffe 0xfffe ) ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 0xfffe 0xfffe 0xfffe 0xfffe 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ) ( v128.const i32x4 0xfffe 0xfffe 0xfffe 0xfffe )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xfffe 0xfffe 0xfffe 0xfffe ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ) ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 ) ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ) ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 ) ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x80000000 -0x80000000 -0x80000000 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0_123_456_789 0_123_456_789 0_123_456_789 0_123_456_789 ) ( v128.const i32x4 01_234_567_890 01_234_567_890 01_234_567_890 01_234_567_890 )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.narrow_i32x4_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef ) ( v128.const i32x4 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i16x8 0xff81 0xff81 0xff81 0xff81 0xff81 0xff81 0xff81 0xff81 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e )))
(local.set $expected ( v128.const i16x8 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f )))
(local.set $expected ( v128.const i16x8 0xff81 0xff81 0xff81 0xff81 0xff81 0xff81 0xff81 0xff81 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i16x8 0x81 0x81 0x81 0x81 0x81 0x81 0x81 0x81 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_low_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e )))
(local.set $expected ( v128.const i16x8 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e 0x7e ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f -0x7f )))
(local.set $expected ( v128.const i16x8 0x81 0x81 0x81 0x81 0x81 0x81 0x81 0x81 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f )))
(local.set $expected ( v128.const i16x8 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.widen_high_i8x16_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i16x8 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7ffe 0x7ffe 0x7ffe 0x7ffe 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7ffe 0x7ffe 0x7ffe 0x7ffe ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7ffe 0x7ffe 0x7ffe 0x7ffe )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x7fff -0x7fff -0x7fff -0x7fff -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8001 0xffff8001 0xffff8001 0xffff8001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x7fff -0x7fff -0x7fff -0x7fff )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 )))
(local.set $expected ( v128.const i32x4 0xffffddd5 0xffffddd5 0xffffddd5 0xffffddd5 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB )))
(local.set $expected ( v128.const i32x4 0xffff90ab 0xffff90ab 0xffff90ab 0xffff90ab ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7ffe 0x7ffe 0x7ffe 0x7ffe 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7ffe 0x7ffe 0x7ffe 0x7ffe )))
(local.set $expected ( v128.const i32x4 0x7ffe 0x7ffe 0x7ffe 0x7ffe ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x7fff -0x7fff -0x7fff -0x7fff -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x7fff -0x7fff -0x7fff -0x7fff )))
(local.set $expected ( v128.const i32x4 0xffff8001 0xffff8001 0xffff8001 0xffff8001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0xffff8000 0xffff8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 )))
(local.set $expected ( v128.const i32x4 0xffffddd5 0xffffddd5 0xffffddd5 0xffffddd5 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_s" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB )))
(local.set $expected ( v128.const i32x4 0xffff90ab 0xffff90ab 0xffff90ab 0xffff90ab ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7ffe 0x7ffe 0x7ffe 0x7ffe 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7ffe 0x7ffe 0x7ffe 0x7ffe ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7ffe 0x7ffe 0x7ffe 0x7ffe )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x7fff -0x7fff -0x7fff -0x7fff -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i32x4 0x8001 0x8001 0x8001 0x8001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x7fff -0x7fff -0x7fff -0x7fff )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 )))
(local.set $expected ( v128.const i32x4 0x0000ddd5 0x0000ddd5 0x0000ddd5 0x0000ddd5 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_low_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB )))
(local.set $expected ( v128.const i32x4 0x000090ab 0x000090ab 0x000090ab 0x000090ab ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7ffe 0x7ffe 0x7ffe 0x7ffe 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7ffe 0x7ffe 0x7ffe 0x7ffe )))
(local.set $expected ( v128.const i32x4 0x7ffe 0x7ffe 0x7ffe 0x7ffe ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x7fff -0x7fff -0x7fff -0x7fff -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x7fff -0x7fff -0x7fff -0x7fff )))
(local.set $expected ( v128.const i32x4 0x8001 0x8001 0x8001 0x8001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i32x4 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i32x4 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 )))
(local.set $expected ( v128.const i32x4 0x0000ddd5 0x0000ddd5 0x0000ddd5 0x0000ddd5 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.widen_high_i16x8_u" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB )))
(local.set $expected ( v128.const i32x4 0x000090ab 0x000090ab 0x000090ab 0x000090ab ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.trunc_sat_f32x4 (v128.const f32x4 0.0 0.0 0.0 0.0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.trunc_s_sat_f32x4 (v128.const f32x4 -2.0 -1.0 1.0 2.0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.trunc_u_sat_f32x4 (v128.const f32x4 -2.0 -1.0 1.0 2.0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.convert_f32x4 (v128.const f32x4 -1 0 1 2))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.convert_s_f32x4 (v128.const f32x4 -1 0 1 2))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.convert_u_f32x4 (v128.const f32x4 -1 0 1 2))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i64x2.trunc_sat_f64x2_s (v128.const f64x2 0.0 0.0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i64x2.trunc_sat_f64x2_u (v128.const f64x2 -2.0 -1.0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (f64x2.convert_i64x2_s (v128.const i64x2 1 2))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (f64x2.convert_i64x2_u (v128.const i64x2 1 2))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i8x16.narrow_i16x8 (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.narrow_i8x16 (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.narrow_i8x16_s (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.narrow_i8x16_u (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.narrow_i32x4 (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.narrow_i16x8 (v128.const i16x8 0 0 0 0 0 0 0 0) (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.narrow_i16x8_s (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.narrow_i16x8_u (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.widen_low_i8x16 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i8x16.widen_low_i16x8_s (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i8x16.widen_low_i16x8_u (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.widen_high_i8x16 (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i8x16.widen_high_i16x8_s (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i8x16.widen_high_i16x8_u (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.widen_low_i16x8 (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.widen_low_i32x4_s (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.widen_low_i32x4_u (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i32x4.widen_high_i16x8 (v128.const i16x8 0 0 0 0 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.widen_high_i32x4_s (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (i16x8.widen_high_i32x4_u (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.trunc_sat_f32x4_s ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.trunc_sat_f32x4_s ( i64.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.trunc_sat_f32x4_u ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.trunc_sat_f32x4_u ( i64.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.convert_i32x4_s ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.convert_i32x4_s ( i64.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.convert_i32x4_u ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.convert_i32x4_u ( i64.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.narrow_i16x8_s ( i32.const 0 ) ( i64.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.narrow_i16x8_u ( i32.const 0 ) ( i64.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.narrow_i32x4_s ( f32.const 0.0 ) ( f64.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.narrow_i32x4_s ( f32.const 0.0 ) ( f64.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.widen_low_i8x16_s ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.widen_high_i8x16_s ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.widen_low_i8x16_u ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.widen_high_i8x16_u ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.widen_low_i16x8_s ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.widen_high_i16x8_s ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.widen_low_i16x8_u ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.widen_high_i16x8_u ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "f32x4_convert_i32x4_s_add" ) ( param v128 v128 ) ( result v128 ) ( f32x4.convert_i32x4_s ( i32x4.add ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "f32x4_convert_i32x4_s_sub" ) ( param v128 v128 ) ( result v128 ) ( f32x4.convert_i32x4_s ( i32x4.sub ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "f32x4_convert_i32x4_u_mul" ) ( param v128 v128 ) ( result v128 ) ( f32x4.convert_i32x4_u ( i32x4.mul ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_low_widen_narrow_ss" ) ( param v128 v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_s ( i8x16.narrow_i16x8_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_low_widen_narrow_su" ) ( param v128 v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_s ( i8x16.narrow_i16x8_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_high_widen_narrow_ss" ) ( param v128 v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_s ( i8x16.narrow_i16x8_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_high_widen_narrow_su" ) ( param v128 v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_s ( i8x16.narrow_i16x8_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_low_widen_narrow_uu" ) ( param v128 v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_u ( i8x16.narrow_i16x8_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_low_widen_narrow_us" ) ( param v128 v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_u ( i8x16.narrow_i16x8_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_high_widen_narrow_uu" ) ( param v128 v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_u ( i8x16.narrow_i16x8_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i16x8_high_widen_narrow_us" ) ( param v128 v128 ) ( result v128 ) ( i16x8.widen_low_i8x16_u ( i8x16.narrow_i16x8_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_low_widen_narrow_ss" ) ( param v128 v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_s ( i16x8.narrow_i32x4_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_low_widen_narrow_su" ) ( param v128 v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_s ( i16x8.narrow_i32x4_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_high_widen_narrow_ss" ) ( param v128 v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_s ( i16x8.narrow_i32x4_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_high_widen_narrow_su" ) ( param v128 v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_s ( i16x8.narrow_i32x4_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_low_widen_narrow_uu" ) ( param v128 v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_u ( i16x8.narrow_i32x4_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_low_widen_narrow_us" ) ( param v128 v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_u ( i16x8.narrow_i32x4_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_high_widen_narrow_uu" ) ( param v128 v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_u ( i16x8.narrow_i32x4_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4_high_widen_narrow_us" ) ( param v128 v128 ) ( result v128 ) ( i32x4.widen_low_i16x8_u ( i16x8.narrow_i32x4_s ( local.get 0 ) ( local.get 1 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_convert_i32x4_s_add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 2 3 4 ) ( v128.const i32x4 2 3 4 5 )))
(local.set $expected ( v128.const f32x4 3.0 5.0 7.0 9.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_convert_i32x4_s_sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 2 3 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const f32x4 -1.0 0.0 1.0 2.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4_convert_i32x4_u_mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 2 3 4 ) ( v128.const i32x4 1 2 3 4 )))
(local.set $expected ( v128.const f32x4 1.0 4.0 9.0 16.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_low_widen_narrow_ss" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0x8000 -0x8000 -0x7fff 0x7fff 0x8000 ) ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0x8000 -0x8000 -0x7fff 0x7fff 0x8000 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0x7f 0xff80 0xff80 0xff80 0x7f 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_low_widen_narrow_su" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0xffff -0x8000 -0x7fff 0x7fff 0xffff ) ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0xffff -0x8000 -0x7fff 0x7fff 0xffff )))
(local.set $expected ( v128.const i16x8 0 0 0xffff 0 0 0 0xffff 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_high_widen_narrow_ss" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0x8000 -0x8000 -0x7fff 0x7fff 0x8000 ) ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0x8000 -0x8000 -0x7fff 0x7fff 0x8000 )))
(local.set $expected ( v128.const i16x8 0xff80 0xff80 0x7f 0xff80 0xff80 0xff80 0x7f 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_high_widen_narrow_su" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0xffff -0x8000 -0x7fff 0x7fff 0xffff ) ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0xffff -0x8000 -0x7fff 0x7fff 0xffff )))
(local.set $expected ( v128.const i16x8 0 0 0xffff 0 0 0 0xffff 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_low_widen_narrow_uu" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x7fff 0x8000 0xffff -0x8000 -0x7fff 0x8000 0xffff ) ( v128.const i16x8 -0x8000 -0x7fff 0x8000 0xffff -0x8000 -0x7fff 0x8000 0xffff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_low_widen_narrow_us" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0x8000 -0x8000 -0x7fff 0x7fff 0x8000 ) ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0x8000 -0x8000 -0x7fff 0x7fff 0x8000 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x7f 0x80 0x80 0x80 0x7f 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_high_widen_narrow_uu" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0xffff -0x8000 -0x7fff 0x7fff 0xffff ) ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0xffff -0x8000 -0x7fff 0x7fff 0xffff )))
(local.set $expected ( v128.const i16x8 0 0 0xff 0 0 0 0xff 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_high_widen_narrow_us" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0x8000 -0x8000 -0x7fff 0x7fff 0x8000 ) ( v128.const i16x8 -0x8000 -0x7fff 0x7fff 0x8000 -0x8000 -0x7fff 0x7fff 0x8000 )))
(local.set $expected ( v128.const i16x8 0x80 0x80 0x7f 0x80 0x80 0x80 0x7f 0x80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_low_widen_narrow_ss" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0x8000000 ) ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0x8000000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_low_widen_narrow_su" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0xffffffff ) ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0 0 0xffffffff 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_high_widen_narrow_ss" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0x8000000 ) ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0x8000000 )))
(local.set $expected ( v128.const i32x4 0xffff8000 0xffff8000 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_high_widen_narrow_su" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0xffffffff ) ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0 0 0xffffffff 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_low_widen_narrow_uu" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0xffffffff ) ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0 0 0xffff 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_low_widen_narrow_us" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0x8000000 ) ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0x8000000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_high_widen_narrow_uu" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0xffffffff ) ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0 0 0xffff 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_high_widen_narrow_us" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0x8000000 ) ( v128.const i32x4 -0x80000000 -0x7fffffff 0x7fffffff 0x8000000 )))
(local.set $expected ( v128.const i32x4 0x8000 0x8000 0x7fff 0x7fff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.trunc_sat_f32x4_s-arg-empty ( result v128 ) ( i32x4.trunc_sat_f32x4_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.trunc_sat_f32x4_u-arg-empty ( result v128 ) ( i32x4.trunc_sat_f32x4_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.convert_i32x4_s-arg-empty ( result v128 ) ( f32x4.convert_i32x4_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.convert_i32x4_u-arg-empty ( result v128 ) ( f32x4.convert_i32x4_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.narrow_i16x8_s-1st-arg-empty ( result v128 ) ( i8x16.narrow_i16x8_s ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.narrow_i16x8_s-arg-empty ( result v128 ) ( i8x16.narrow_i16x8_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.narrow_i16x8_u-1st-arg-empty ( result v128 ) ( i8x16.narrow_i16x8_u ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.narrow_i16x8_u-arg-empty ( result v128 ) ( i8x16.narrow_i16x8_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.narrow_i32x4_s-1st-arg-empty ( result v128 ) ( i16x8.narrow_i32x4_s ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.narrow_i32x4_s-arg-empty ( result v128 ) ( i16x8.narrow_i32x4_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.narrow_i32x4_u-1st-arg-empty ( result v128 ) ( i16x8.narrow_i32x4_u ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.narrow_i32x4_u-arg-empty ( result v128 ) ( i16x8.narrow_i32x4_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.widen_high_i8x16_s-arg-empty ( result v128 ) ( i16x8.widen_high_i8x16_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.widen_high_i8x16_u-arg-empty ( result v128 ) ( i16x8.widen_high_i8x16_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.widen_low_i8x16_s-arg-empty ( result v128 ) ( i16x8.widen_low_i8x16_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.widen_low_i8x16_u-arg-empty ( result v128 ) ( i16x8.widen_low_i8x16_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.widen_high_i16x8_s-arg-empty ( result v128 ) ( i32x4.widen_high_i16x8_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.widen_high_i16x8_u-arg-empty ( result v128 ) ( i32x4.widen_high_i16x8_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.widen_low_i16x8_s-arg-empty ( result v128 ) ( i32x4.widen_low_i16x8_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.widen_low_i16x8_u-arg-empty ( result v128 ) ( i32x4.widen_low_i16x8_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)

