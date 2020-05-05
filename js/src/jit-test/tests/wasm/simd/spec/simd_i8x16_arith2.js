var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i8x16.min_s" ) ( param v128 v128 ) ( result v128 ) ( i8x16.min_s ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i8x16.min_u" ) ( param v128 v128 ) ( result v128 ) ( i8x16.min_u ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i8x16.max_s" ) ( param v128 v128 ) ( result v128 ) ( i8x16.max_s ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i8x16.max_u" ) ( param v128 v128 ) ( result v128 ) ( i8x16.max_u ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i8x16.avgr_u" ) ( param v128 v128 ) ( result v128 ) ( i8x16.avgr_u ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i8x16.abs" ) ( param v128 ) ( result v128 ) ( i8x16.abs ( local.get 0 ) ) ) ( func ( export "i8x16.min_s_with_const_0" ) ( result v128 ) ( i8x16.min_s ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 ) ) ) ( func ( export "i8x16.min_s_with_const_1" ) ( result v128 ) ( i8x16.min_s ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 ) ) ) ( func ( export "i8x16.min_u_with_const_2" ) ( result v128 ) ( i8x16.min_u ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 ) ) ) ( func ( export "i8x16.min_u_with_const_3" ) ( result v128 ) ( i8x16.min_u ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 ) ) ) ( func ( export "i8x16.max_s_with_const_4" ) ( result v128 ) ( i8x16.max_s ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 ) ) ) ( func ( export "i8x16.max_s_with_const_5" ) ( result v128 ) ( i8x16.max_s ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 ) ) ) ( func ( export "i8x16.max_u_with_const_6" ) ( result v128 ) ( i8x16.max_u ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 ) ) ) ( func ( export "i8x16.max_u_with_const_7" ) ( result v128 ) ( i8x16.max_u ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 ) ) ) ( func ( export "i8x16.avgr_u_with_const_8" ) ( result v128 ) ( i8x16.avgr_u ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 ) ) ) ( func ( export "i8x16.avgr_u_with_const_9" ) ( result v128 ) ( i8x16.avgr_u ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 ) ) ) ( func ( export "i8x16.abs_with_const_10" ) ( result v128 ) ( i8x16.abs ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ) ) ( func ( export "i8x16.min_s_with_const_11" ) ( param v128 ) ( result v128 ) ( i8x16.min_s ( local.get 0 ) ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ) ) ( func ( export "i8x16.min_s_with_const_12" ) ( param v128 ) ( result v128 ) ( i8x16.min_s ( local.get 0 ) ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ) ) ( func ( export "i8x16.min_u_with_const_13" ) ( param v128 ) ( result v128 ) ( i8x16.min_u ( local.get 0 ) ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ) ) ( func ( export "i8x16.min_u_with_const_14" ) ( param v128 ) ( result v128 ) ( i8x16.min_u ( local.get 0 ) ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ) ) ( func ( export "i8x16.max_s_with_const_15" ) ( param v128 ) ( result v128 ) ( i8x16.max_s ( local.get 0 ) ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ) ) ( func ( export "i8x16.max_s_with_const_16" ) ( param v128 ) ( result v128 ) ( i8x16.max_s ( local.get 0 ) ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ) ) ( func ( export "i8x16.max_u_with_const_17" ) ( param v128 ) ( result v128 ) ( i8x16.max_u ( local.get 0 ) ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ) ) ( func ( export "i8x16.max_u_with_const_18" ) ( param v128 ) ( result v128 ) ( i8x16.max_u ( local.get 0 ) ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ) ) ( func ( export "i8x16.avgr_u_with_const_19" ) ( param v128 ) ( result v128 ) ( i8x16.avgr_u ( local.get 0 ) ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ) ) ( func ( export "i8x16.avgr_u_with_const_20" ) ( param v128 ) ( result v128 ) ( i8x16.avgr_u ( local.get 0 ) ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 3 3 3 3 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 -1 -1 -1 -1 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ) ( v128.const i8x16 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 -1 -1 -1 -1 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ) ( v128.const i8x16 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 -1 -1 -1 -1 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ) ( v128.const i8x16 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 -1 -1 -1 -1 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ) ( v128.const i8x16 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 -1 -1 -1 -1 0 0 0 0 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 128 128 128 128 128 128 128 128 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 192 192 192 192 192 192 192 192 192 192 192 192 192 192 192 192 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ) ( v128.const i8x16 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff )))
(local.set $expected ( v128.const i8x16 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 0x1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i8x16 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 01_2_3 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 -01_2_3 )))
(local.set $expected ( v128.const i8x16 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 123 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s_with_const_0" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 -128 -128 -128 -128 64 64 64 64 64 64 64 64 -128 -128 -128 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s_with_const_1" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u_with_const_2" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 -128 -128 -128 -128 64 64 64 64 64 64 64 64 -128 -128 -128 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u_with_const_3" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s_with_const_4" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 255 255 255 255 127 127 127 127 127 127 127 127 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s_with_const_5" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 3 3 3 3 2 2 2 2 2 2 2 2 3 3 3 3 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u_with_const_6" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 255 255 255 255 127 127 127 127 127 127 127 127 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u_with_const_7" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 3 3 3 3 2 2 2 2 2 2 2 2 3 3 3 3 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u_with_const_8" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 192 192 192 192 96 96 96 96 96 96 96 96 192 192 192 192 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u_with_const_9" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs_with_const_10" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 128 128 128 128 127 127 127 127 64 64 64 64 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s_with_const_11" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -128 -128 -128 -128 64 64 64 64 64 64 64 64 -128 -128 -128 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s_with_const_12" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u_with_const_13" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -128 -128 -128 -128 64 64 64 64 64 64 64 64 -128 -128 -128 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u_with_const_14" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 1 1 1 1 1 1 1 1 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s_with_const_15" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 255 255 255 255 127 127 127 127 127 127 127 127 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s_with_const_16" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 3 3 3 3 2 2 2 2 2 2 2 2 3 3 3 3 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u_with_const_17" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 255 255 255 255 127 127 127 127 127 127 127 127 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u_with_const_18" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 3 3 3 3 2 2 2 2 2 2 2 2 3 3 3 3 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u_with_const_19" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 192 192 192 192 96 96 96 96 96 96 96 96 192 192 192 192 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u_with_const_20" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 3 3 3 3 2 2 2 2 1 1 1 1 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -128 -128 -128 -128 64 64 64 64 64 64 64 64 -128 -128 -128 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 128 128 128 128 ) ( v128.const i8x16 0 0 0 0 2 2 2 2 1 1 1 1 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 1 1 1 1 1 1 1 1 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -128 -128 -128 -128 64 64 64 64 64 64 64 64 -128 -128 -128 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 128 128 128 128 ) ( v128.const i8x16 0 0 0 0 2 2 2 2 1 1 1 1 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 1 1 1 1 1 1 1 1 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 255 255 255 255 127 127 127 127 127 127 127 127 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 128 128 128 128 ) ( v128.const i8x16 0 0 0 0 2 2 2 2 1 1 1 1 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 2 2 2 2 2 2 2 2 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 255 255 255 255 127 127 127 127 127 127 127 127 255 255 255 255 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 128 128 128 128 ) ( v128.const i8x16 0 0 0 0 2 2 2 2 1 1 1 1 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 2 2 2 2 2 2 2 2 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 64 64 64 64 127 127 127 127 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 192 192 192 192 96 96 96 96 96 96 96 96 192 192 192 192 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 2 2 128 128 128 128 ) ( v128.const i8x16 0 0 0 0 2 2 2 2 1 1 1 1 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 2 2 2 2 2 2 2 2 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 127 127 127 127 64 64 64 64 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 127 127 127 127 64 64 64 64 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ) ( v128.const i8x16 +0 +0 +0 +0 0 0 0 0 -0 -0 -0 -0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ) ( v128.const i8x16 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ) ( v128.const i8x16 +0 +0 +0 +0 0 0 0 0 -0 -0 -0 -0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ) ( v128.const i8x16 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ) ( v128.const i8x16 +0 +0 +0 +0 0 0 0 0 -0 -0 -0 -0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ) ( v128.const i8x16 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ) ( v128.const i8x16 +0 +0 +0 +0 0 0 0 0 -0 -0 -0 -0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ) ( v128.const i8x16 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ) ( v128.const i8x16 +0 +0 +0 +0 0 0 0 0 -0 -0 -0 -0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ) ( v128.const i8x16 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 +0 +0 +0 +0 +0 +0 +0 +0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 +0 +0 +0 +0 0 0 0 0 -0 -0 -0 -0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 +0 +0 +0 +0 0 0 0 0 -0 -0 -0 -0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 )))
(local.set $expected ( v128.const i8x16 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 -0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i8x16 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 +0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i32x4.avgr_u (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f32x4.avgr_u (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.avgr_u (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f64x2.avgr_u (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i8x16.avgr (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i8x16.avgr_s (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0) (v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.min_s ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.min_u ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.max_s ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.max_u ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.avgr_u ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.abs ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.min_s-1st-arg-empty ( result v128 ) ( i8x16.min_s ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.min_s-arg-empty ( result v128 ) ( i8x16.min_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.min_u-1st-arg-empty ( result v128 ) ( i8x16.min_u ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.min_u-arg-empty ( result v128 ) ( i8x16.min_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.max_s-1st-arg-empty ( result v128 ) ( i8x16.max_s ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.max_s-arg-empty ( result v128 ) ( i8x16.max_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.max_u-1st-arg-empty ( result v128 ) ( i8x16.max_u ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.max_u-arg-empty ( result v128 ) ( i8x16.max_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.avgr_u-1st-arg-empty ( result v128 ) ( i8x16.avgr_u ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.avgr_u-arg-empty ( result v128 ) ( i8x16.avgr_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.abs-arg-empty ( result v128 ) ( i8x16.abs ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i8x16.min_s-i8x16.avgr_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_s ( i8x16.avgr_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_s-i8x16.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_s ( i8x16.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_s-i8x16.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_s ( i8x16.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_s-i8x16.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_s ( i8x16.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_s-i8x16.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_s ( i8x16.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_s-i8x16.abs" ) ( param v128 v128 ) ( result v128 ) ( i8x16.min_s ( i8x16.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i8x16.abs-i8x16.min_s" ) ( param v128 v128 ) ( result v128 ) ( i8x16.abs ( i8x16.min_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i8x16.min_u-i8x16.avgr_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_u ( i8x16.avgr_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_u-i8x16.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_u ( i8x16.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_u-i8x16.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_u ( i8x16.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_u-i8x16.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_u ( i8x16.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_u-i8x16.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.min_u ( i8x16.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.min_u-i8x16.abs" ) ( param v128 v128 ) ( result v128 ) ( i8x16.min_u ( i8x16.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i8x16.abs-i8x16.min_u" ) ( param v128 v128 ) ( result v128 ) ( i8x16.abs ( i8x16.min_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i8x16.max_s-i8x16.avgr_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_s ( i8x16.avgr_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_s-i8x16.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_s ( i8x16.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_s-i8x16.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_s ( i8x16.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_s-i8x16.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_s ( i8x16.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_s-i8x16.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_s ( i8x16.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_s-i8x16.abs" ) ( param v128 v128 ) ( result v128 ) ( i8x16.max_s ( i8x16.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i8x16.abs-i8x16.max_s" ) ( param v128 v128 ) ( result v128 ) ( i8x16.abs ( i8x16.max_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i8x16.max_u-i8x16.avgr_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_u ( i8x16.avgr_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_u-i8x16.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_u ( i8x16.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_u-i8x16.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_u ( i8x16.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_u-i8x16.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_u ( i8x16.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_u-i8x16.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.max_u ( i8x16.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.max_u-i8x16.abs" ) ( param v128 v128 ) ( result v128 ) ( i8x16.max_u ( i8x16.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i8x16.abs-i8x16.max_u" ) ( param v128 v128 ) ( result v128 ) ( i8x16.abs ( i8x16.max_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i8x16.avgr_u-i8x16.avgr_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.avgr_u ( i8x16.avgr_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.avgr_u-i8x16.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.avgr_u ( i8x16.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.avgr_u-i8x16.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.avgr_u ( i8x16.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.avgr_u-i8x16.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.avgr_u ( i8x16.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.avgr_u-i8x16.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i8x16.avgr_u ( i8x16.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i8x16.avgr_u-i8x16.abs" ) ( param v128 v128 ) ( result v128 ) ( i8x16.avgr_u ( i8x16.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i8x16.abs-i8x16.avgr_u" ) ( param v128 v128 ) ( result v128 ) ( i8x16.abs ( i8x16.avgr_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i8x16.abs-i8x16.abs" ) ( param v128 ) ( result v128 ) ( i8x16.abs ( i8x16.abs ( local.get 0 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s-i8x16.avgr_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s-i8x16.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s-i8x16.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s-i8x16.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s-i8x16.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_s-i8x16.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs-i8x16.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u-i8x16.avgr_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u-i8x16.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u-i8x16.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u-i8x16.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u-i8x16.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.min_u-i8x16.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs-i8x16.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s-i8x16.avgr_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s-i8x16.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s-i8x16.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s-i8x16.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s-i8x16.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_s-i8x16.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs-i8x16.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u-i8x16.avgr_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u-i8x16.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u-i8x16.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u-i8x16.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u-i8x16.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.max_u-i8x16.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs-i8x16.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u-i8x16.avgr_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u-i8x16.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u-i8x16.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u-i8x16.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u-i8x16.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.avgr_u-i8x16.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs-i8x16.avgr_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.abs-i8x16.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)

