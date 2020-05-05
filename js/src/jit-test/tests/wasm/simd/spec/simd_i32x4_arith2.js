var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i32x4.min_s" ) ( param v128 v128 ) ( result v128 ) ( i32x4.min_s ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i32x4.min_u" ) ( param v128 v128 ) ( result v128 ) ( i32x4.min_u ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i32x4.max_s" ) ( param v128 v128 ) ( result v128 ) ( i32x4.max_s ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i32x4.max_u" ) ( param v128 v128 ) ( result v128 ) ( i32x4.max_u ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i32x4.abs" ) ( param v128 ) ( result v128 ) ( i32x4.abs ( local.get 0 ) ) ) ( func ( export "i32x4.min_s_with_const_0" ) ( result v128 ) ( i32x4.min_s ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 ) ) ) ( func ( export "i32x4.min_s_with_const_1" ) ( result v128 ) ( i32x4.min_s ( v128.const i32x4 0 1 2 3 ) ( v128.const i32x4 3 2 1 0 ) ) ) ( func ( export "i32x4.min_u_with_const_2" ) ( result v128 ) ( i32x4.min_u ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 ) ) ) ( func ( export "i32x4.min_u_with_const_3" ) ( result v128 ) ( i32x4.min_u ( v128.const i32x4 0 1 2 3 ) ( v128.const i32x4 3 2 1 0 ) ) ) ( func ( export "i32x4.max_s_with_const_4" ) ( result v128 ) ( i32x4.max_s ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 ) ) ) ( func ( export "i32x4.max_s_with_const_5" ) ( result v128 ) ( i32x4.max_s ( v128.const i32x4 0 1 2 3 ) ( v128.const i32x4 3 2 1 0 ) ) ) ( func ( export "i32x4.max_u_with_const_6" ) ( result v128 ) ( i32x4.max_u ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 ) ) ) ( func ( export "i32x4.max_u_with_const_7" ) ( result v128 ) ( i32x4.max_u ( v128.const i32x4 0 1 2 3 ) ( v128.const i32x4 3 2 1 0 ) ) ) ( func ( export "i32x4.abs_with_const_8" ) ( result v128 ) ( i32x4.abs ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ) ) ( func ( export "i32x4.min_s_with_const_9" ) ( param v128 ) ( result v128 ) ( i32x4.min_s ( local.get 0 ) ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ) ) ( func ( export "i32x4.min_s_with_const_10" ) ( param v128 ) ( result v128 ) ( i32x4.min_s ( local.get 0 ) ( v128.const i32x4 0 1 2 3 ) ) ) ( func ( export "i32x4.min_u_with_const_11" ) ( param v128 ) ( result v128 ) ( i32x4.min_u ( local.get 0 ) ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ) ) ( func ( export "i32x4.min_u_with_const_12" ) ( param v128 ) ( result v128 ) ( i32x4.min_u ( local.get 0 ) ( v128.const i32x4 0 1 2 3 ) ) ) ( func ( export "i32x4.max_s_with_const_13" ) ( param v128 ) ( result v128 ) ( i32x4.max_s ( local.get 0 ) ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ) ) ( func ( export "i32x4.max_s_with_const_14" ) ( param v128 ) ( result v128 ) ( i32x4.max_s ( local.get 0 ) ( v128.const i32x4 0 1 2 3 ) ) ) ( func ( export "i32x4.max_u_with_const_15" ) ( param v128 ) ( result v128 ) ( i32x4.max_u ( local.get 0 ) ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ) ) ( func ( export "i32x4.max_u_with_const_16" ) ( param v128 ) ( result v128 ) ( i32x4.max_u ( local.get 0 ) ( v128.const i32x4 0 1 2 3 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 -1 ) ( v128.const i32x4 0 -1 0 -1 )))
(local.set $expected ( v128.const i32x4 0 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ) ( v128.const i32x4 128 128 128 128 )))
(local.set $expected ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ) ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ) ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 123 123 123 123 ) ( v128.const i32x4 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i32x4 123 123 123 123 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80 0x80 0x80 0x80 ) ( v128.const i32x4 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i32x4 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 -1 ) ( v128.const i32x4 0 -1 0 -1 )))
(local.set $expected ( v128.const i32x4 0 0 0 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ) ( v128.const i32x4 128 128 128 128 )))
(local.set $expected ( v128.const i32x4 128 128 128 128 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ) ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ) ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 123 123 123 123 ) ( v128.const i32x4 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i32x4 123 123 123 123 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80 0x80 0x80 0x80 ) ( v128.const i32x4 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i32x4 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 -1 ) ( v128.const i32x4 0 -1 0 -1 )))
(local.set $expected ( v128.const i32x4 0 0 0 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ) ( v128.const i32x4 128 128 128 128 )))
(local.set $expected ( v128.const i32x4 128 128 128 128 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ) ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ) ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 123 123 123 123 ) ( v128.const i32x4 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i32x4 123 123 123 123 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80 0x80 0x80 0x80 ) ( v128.const i32x4 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i32x4 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 -1 ) ( v128.const i32x4 0 -1 0 -1 )))
(local.set $expected ( v128.const i32x4 0 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ) ( v128.const i32x4 128 128 128 128 )))
(local.set $expected ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ) ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ) ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 123 123 123 123 ) ( v128.const i32x4 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i32x4 123 123 123 123 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80 0x80 0x80 0x80 ) ( v128.const i32x4 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i32x4 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i32x4 0x1 0x1 0x1 0x1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -2147483648 -2147483648 -2147483648 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483648 2147483648 2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80000000 -0x80000000 -0x80000000 -0x80000000 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 01_2_3 01_2_3 01_2_3 01_2_3 )))
(local.set $expected ( v128.const i32x4 01_2_3 01_2_3 01_2_3 01_2_3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -01_2_3 -01_2_3 -01_2_3 -01_2_3 )))
(local.set $expected ( v128.const i32x4 123 123 123 123 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x80 0x80 0x80 0x80 )))
(local.set $expected ( v128.const i32x4 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x80 -0x80 -0x80 -0x80 )))
(local.set $expected ( v128.const i32x4 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 )))
(local.set $expected ( v128.const i32x4 0x0_8_0 0x0_8_0 0x0_8_0 0x0_8_0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0x0_8_0 -0x0_8_0 -0x0_8_0 -0x0_8_0 )))
(local.set $expected ( v128.const i32x4 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s_with_const_0" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 -2147483648 1073741824 1073741824 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s_with_const_1" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 0 1 1 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u_with_const_2" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 -2147483648 1073741824 1073741824 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u_with_const_3" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 0 1 1 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s_with_const_4" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 4294967295 2147483647 2147483647 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s_with_const_5" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 3 2 2 3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u_with_const_6" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 4294967295 2147483647 2147483647 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u_with_const_7" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 3 2 2 3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs_with_const_8" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 2147483648 2147483647 1073741824 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s_with_const_9" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 )))
(local.set $expected ( v128.const i32x4 -2147483648 1073741824 1073741824 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s_with_const_10" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 3 2 1 0 )))
(local.set $expected ( v128.const i32x4 0 1 1 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u_with_const_11" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 )))
(local.set $expected ( v128.const i32x4 -2147483648 1073741824 1073741824 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u_with_const_12" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 3 2 1 0 )))
(local.set $expected ( v128.const i32x4 0 1 1 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s_with_const_13" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 )))
(local.set $expected ( v128.const i32x4 4294967295 2147483647 2147483647 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s_with_const_14" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 3 2 1 0 )))
(local.set $expected ( v128.const i32x4 3 2 2 3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u_with_const_15" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 )))
(local.set $expected ( v128.const i32x4 4294967295 2147483647 2147483647 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u_with_const_16" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 3 2 1 0 )))
(local.set $expected ( v128.const i32x4 3 2 2 3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 )))
(local.set $expected ( v128.const i32x4 -2147483648 1073741824 1073741824 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 2 128 ) ( v128.const i32x4 0 2 1 128 )))
(local.set $expected ( v128.const i32x4 0 1 1 128 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 )))
(local.set $expected ( v128.const i32x4 -2147483648 1073741824 1073741824 -2147483648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 2 128 ) ( v128.const i32x4 0 2 1 128 )))
(local.set $expected ( v128.const i32x4 0 1 1 128 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 )))
(local.set $expected ( v128.const i32x4 4294967295 2147483647 2147483647 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 2 128 ) ( v128.const i32x4 0 2 1 128 )))
(local.set $expected ( v128.const i32x4 0 2 2 128 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 ) ( v128.const i32x4 4294967295 1073741824 2147483647 -2147483648 )))
(local.set $expected ( v128.const i32x4 4294967295 2147483647 2147483647 4294967295 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 2 128 ) ( v128.const i32x4 0 2 1 128 )))
(local.set $expected ( v128.const i32x4 0 2 2 128 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 2147483647 1073741824 4294967295 )))
(local.set $expected ( v128.const i32x4 2147483648 2147483647 1073741824 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 +0 +0 ) ( v128.const i32x4 +0 0 -0 0 )))
(local.set $expected ( v128.const i32x4 -0 -0 +0 +0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 -0 -0 ) ( v128.const i32x4 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i32x4 -0 -0 -0 -0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 +0 +0 ) ( v128.const i32x4 +0 0 -0 0 )))
(local.set $expected ( v128.const i32x4 -0 -0 +0 +0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 -0 -0 ) ( v128.const i32x4 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i32x4 -0 -0 -0 -0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 +0 +0 ) ( v128.const i32x4 +0 0 -0 0 )))
(local.set $expected ( v128.const i32x4 -0 -0 +0 +0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 -0 -0 ) ( v128.const i32x4 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i32x4 -0 -0 -0 -0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 +0 +0 ) ( v128.const i32x4 +0 0 -0 0 )))
(local.set $expected ( v128.const i32x4 -0 -0 +0 +0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 -0 -0 ) ( v128.const i32x4 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i32x4 -0 -0 -0 -0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 +0 +0 )))
(local.set $expected ( v128.const i32x4 -0 -0 +0 +0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 +0 0 -0 0 )))
(local.set $expected ( v128.const i32x4 +0 0 -0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -0 -0 -0 -0 )))
(local.set $expected ( v128.const i32x4 -0 -0 -0 -0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 +0 +0 +0 +0 )))
(local.set $expected ( v128.const i32x4 +0 +0 +0 +0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f32x4.min_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f32x4.min_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f32x4.max_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f32x4.max_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.min_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.min_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.max_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.max_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f64x2.min_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f64x2.min_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f64x2.max_s (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f64x2.max_u (v128.const i32x4 0 0 0 0) (v128.const i32x4 1 1 1 1))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.min_s ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.min_u ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.max_s ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.max_u ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.abs ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.min_s-1st-arg-empty ( result v128 ) ( i32x4.min_s ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.min_s-arg-empty ( result v128 ) ( i32x4.min_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.min_u-1st-arg-empty ( result v128 ) ( i32x4.min_u ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.min_u-arg-empty ( result v128 ) ( i32x4.min_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.max_s-1st-arg-empty ( result v128 ) ( i32x4.max_s ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.max_s-arg-empty ( result v128 ) ( i32x4.max_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.max_u-1st-arg-empty ( result v128 ) ( i32x4.max_u ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.max_u-arg-empty ( result v128 ) ( i32x4.max_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.abs-arg-empty ( result v128 ) ( i32x4.abs ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i32x4.min_s-i32x4.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.min_s ( i32x4.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.min_s-i32x4.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.min_s ( i32x4.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.min_s-i32x4.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.min_s ( i32x4.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.min_s-i32x4.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.min_s ( i32x4.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.min_s-i32x4.abs" ) ( param v128 v128 ) ( result v128 ) ( i32x4.min_s ( i32x4.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i32x4.abs-i32x4.min_s" ) ( param v128 v128 ) ( result v128 ) ( i32x4.abs ( i32x4.min_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4.min_u-i32x4.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.min_u ( i32x4.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.min_u-i32x4.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.min_u ( i32x4.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.min_u-i32x4.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.min_u ( i32x4.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.min_u-i32x4.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.min_u ( i32x4.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.min_u-i32x4.abs" ) ( param v128 v128 ) ( result v128 ) ( i32x4.min_u ( i32x4.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i32x4.abs-i32x4.min_u" ) ( param v128 v128 ) ( result v128 ) ( i32x4.abs ( i32x4.min_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4.max_s-i32x4.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.max_s ( i32x4.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.max_s-i32x4.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.max_s ( i32x4.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.max_s-i32x4.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.max_s ( i32x4.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.max_s-i32x4.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.max_s ( i32x4.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.max_s-i32x4.abs" ) ( param v128 v128 ) ( result v128 ) ( i32x4.max_s ( i32x4.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i32x4.abs-i32x4.max_s" ) ( param v128 v128 ) ( result v128 ) ( i32x4.abs ( i32x4.max_s ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4.max_u-i32x4.max_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.max_u ( i32x4.max_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.max_u-i32x4.max_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.max_u ( i32x4.max_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.max_u-i32x4.min_u" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.max_u ( i32x4.min_u ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.max_u-i32x4.min_s" ) ( param v128 v128 v128 ) ( result v128 ) ( i32x4.max_u ( i32x4.min_s ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "i32x4.max_u-i32x4.abs" ) ( param v128 v128 ) ( result v128 ) ( i32x4.max_u ( i32x4.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "i32x4.abs-i32x4.max_u" ) ( param v128 v128 ) ( result v128 ) ( i32x4.abs ( i32x4.max_u ( local.get 0 ) ( local.get 1 ) ) ) ) ( func ( export "i32x4.abs-i32x4.abs" ) ( param v128 ) ( result v128 ) ( i32x4.abs ( i32x4.abs ( local.get 0 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s-i32x4.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s-i32x4.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s-i32x4.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s-i32x4.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_s-i32x4.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs-i32x4.min_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u-i32x4.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u-i32x4.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u-i32x4.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u-i32x4.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.min_u-i32x4.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs-i32x4.min_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s-i32x4.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s-i32x4.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s-i32x4.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s-i32x4.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_s-i32x4.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs-i32x4.max_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u-i32x4.max_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u-i32x4.max_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u-i32x4.min_u" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u-i32x4.min_s" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 2 2 2 2 )))
(local.set $expected ( v128.const i32x4 2 2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.max_u-i32x4.abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs-i32x4.max_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.abs-i32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)

