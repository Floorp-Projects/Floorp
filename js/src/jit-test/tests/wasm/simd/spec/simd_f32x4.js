var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "f32x4.min" ) ( param v128 v128 ) ( result v128 ) ( f32x4.min ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "f32x4.max" ) ( param v128 v128 ) ( result v128 ) ( f32x4.max ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "f32x4.abs" ) ( param v128 ) ( result v128 ) ( f32x4.abs ( local.get 0 ) ) ) ( func ( export "f32x4.min_with_const_0" ) ( result v128 ) ( f32x4.min ( v128.const f32x4 0 1 2 -3 ) ( v128.const f32x4 0 2 1 3 ) ) ) ( func ( export "f32x4.min_with_const_1" ) ( result v128 ) ( f32x4.min ( v128.const f32x4 0 1 2 3 ) ( v128.const f32x4 0 1 2 3 ) ) ) ( func ( export "f32x4.min_with_const_2" ) ( result v128 ) ( f32x4.min ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ( v128.const f32x4 0x00 0x02 0x01 2147483648 ) ) ) ( func ( export "f32x4.min_with_const_3" ) ( result v128 ) ( f32x4.min ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ) ) ( func ( export "f32x4.min_with_const_5" ) ( param v128 ) ( result v128 ) ( f32x4.min ( local.get 0 ) ( v128.const f32x4 0 1 2 -3 ) ) ) ( func ( export "f32x4.min_with_const_6" ) ( param v128 ) ( result v128 ) ( f32x4.min ( v128.const f32x4 0 1 2 3 ) ( local.get 0 ) ) ) ( func ( export "f32x4.min_with_const_7" ) ( param v128 ) ( result v128 ) ( f32x4.min ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ( local.get 0 ) ) ) ( func ( export "f32x4.min_with_const_8" ) ( param v128 ) ( result v128 ) ( f32x4.min ( local.get 0 ) ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ) ) ( func ( export "f32x4.max_with_const_10" ) ( result v128 ) ( f32x4.max ( v128.const f32x4 0 1 2 -3 ) ( v128.const f32x4 0 2 1 3 ) ) ) ( func ( export "f32x4.max_with_const_11" ) ( result v128 ) ( f32x4.max ( v128.const f32x4 0 1 2 3 ) ( v128.const f32x4 0 1 2 3 ) ) ) ( func ( export "f32x4.max_with_const_12" ) ( result v128 ) ( f32x4.max ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ( v128.const f32x4 0x00 0x02 0x01 2147483648 ) ) ) ( func ( export "f32x4.max_with_const_13" ) ( result v128 ) ( f32x4.max ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ) ) ( func ( export "f32x4.max_with_const_15" ) ( param v128 ) ( result v128 ) ( f32x4.max ( local.get 0 ) ( v128.const f32x4 0 1 2 -3 ) ) ) ( func ( export "f32x4.max_with_const_16" ) ( param v128 ) ( result v128 ) ( f32x4.max ( v128.const f32x4 0 1 2 3 ) ( local.get 0 ) ) ) ( func ( export "f32x4.max_with_const_17" ) ( param v128 ) ( result v128 ) ( f32x4.max ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ( local.get 0 ) ) ) ( func ( export "f32x4.max_with_const_18" ) ( param v128 ) ( result v128 ) ( f32x4.max ( local.get 0 ) ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ) ) ) ( func ( export "f32x4.abs_with_const" ) ( result v128 ) ( f32x4.abs ( v128.const f32x4 -0 -1 -2 -3 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min_with_const_0" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0 1 1 -3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min_with_const_1" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0 1 2 3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min_with_const_2" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0x00 0x01 0x01 0x80000000 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min_with_const_3" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min_with_const_5" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 2 1 3 )))
(local.set $expected ( v128.const f32x4 0 1 1 -3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min_with_const_6" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 1 2 3 )))
(local.set $expected ( v128.const f32x4 0 1 2 3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min_with_const_7" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x00 0x02 0x01 2147483648 )))
(local.set $expected ( v128.const f32x4 0x00 0x01 0x01 0x80000000 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min_with_const_8" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x00 0x01 0x02 0x80000000 )))
(local.set $expected ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max_with_const_10" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0 2 2 3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max_with_const_11" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0 1 2 3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max_with_const_12" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0x00 0x02 0x02 2147483648 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max_with_const_13" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max_with_const_15" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 2 1 3 )))
(local.set $expected ( v128.const f32x4 0 2 2 3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max_with_const_16" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 1 2 3 )))
(local.set $expected ( v128.const f32x4 0 1 2 3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max_with_const_17" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x00 0x02 0x01 2147483648 )))
(local.set $expected ( v128.const f32x4 0x00 0x02 0x02 2147483648 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max_with_const_18" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x00 0x01 0x02 0x80000000 )))
(local.set $expected ( v128.const f32x4 0x00 0x01 0x02 0x80000000 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs_with_const" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f32x4 0 1 2 3 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan 0 0 1 ) ( v128.const f32x4 0 -nan 1 0 )))
(local.set $expected ( v128.const f32x4 nan nan 0 0 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan 0 0 0 ) ( v128.const f32x4 0 -nan 1 0 )))
(local.set $expected ( v128.const f32x4 nan nan 0 0 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan 0 0 1 ) ( v128.const f32x4 0 -nan 1 0 )))
(local.set $expected ( v128.const f32x4 nan nan 1 1 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan 0 0 0 ) ( v128.const f32x4 0 -nan 1 0 )))
(local.set $expected ( v128.const f32x4 nan nan 1 0 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0xFFFFFFFF  0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 -0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 -0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 -0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 -0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ) ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 nan nan nan nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -nan -nan -nan -nan ) ( v128.const f32x4 -nan -nan -nan -nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 0 -0 +0 ) ( v128.const f32x4 +0 -0 +0 -0 )))
(local.set $expected ( v128.const f32x4 0 -0 -0 -0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0 -0 -0 -0 ) ( v128.const f32x4 +0 +0 +0 +0 )))
(local.set $expected ( v128.const f32x4 -0 -0 -0 -0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0 0 -0 +0 ) ( v128.const f32x4 +0 -0 +0 -0 )))
(local.set $expected ( v128.const f32x4 0 0 0 0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0 -0 -0 -0 ) ( v128.const f32x4 +0 +0 +0 +0 )))
(local.set $expected ( v128.const f32x4 +0 +0 +0 +0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f32x4 0x0.0p+0 0x0.0p+0 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 0x1.0000000000000p-149 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 0x1.0000000000000p-126 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f32x4 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 )))
(local.set $expected ( v128.const f32x4 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 0x1.921fb60000000p+2 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 0x1.fffffe0000000p+127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 inf inf inf inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 )))
(local.set $expected ( v128.const f32x4 0123456789e-019 0123456789e-019 0123456789e-019 0123456789e-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 )))
(local.set $expected ( v128.const f32x4 0123456789.e019 0123456789.e019 0123456789.e019 0123456789.e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 -0123456789.0123456789 )))
(local.set $expected ( v128.const f32x4 0123456789.0123456789 0123456789.0123456789 0123456789.0123456789 0123456789.0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i8x16.min (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i8x16.max (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i16x8.min (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i16x8.max (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i32x4.min (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i32x4.max (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.min (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.max (v128.const i32x4 0 0 0 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.abs ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.min ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f32x4.max ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.abs-arg-empty ( result v128 ) ( f32x4.abs ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.min-1st-arg-empty ( result v128 ) ( f32x4.min ( v128.const f32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.min-arg-empty ( result v128 ) ( f32x4.min ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.max-1st-arg-empty ( result v128 ) ( f32x4.max ( v128.const f32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.max-arg-empty ( result v128 ) ( f32x4.max ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "max-min" ) ( param v128 v128 v128 ) ( result v128 ) ( f32x4.max ( f32x4.min ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "min-max" ) ( param v128 v128 v128 ) ( result v128 ) ( f32x4.min ( f32x4.max ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "max-abs" ) ( param v128 v128 ) ( result v128 ) ( f32x4.max ( f32x4.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "min-abs" ) ( param v128 v128 ) ( result v128 ) ( f32x4.min ( f32x4.abs ( local.get 0 ) ) ( local.get 1 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "max-min" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 1.125 1.125 1.125 1.125 ) ( v128.const f32x4 0.25 0.25 0.25 0.25 ) ( v128.const f32x4 0.125 0.125 0.125 0.125 )))
(local.set $expected ( v128.const f32x4 0.25 0.25 0.25 0.25 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "min-max" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 1.125 1.125 1.125 1.125 ) ( v128.const f32x4 0.25 0.25 0.25 0.25 ) ( v128.const f32x4 0.125 0.125 0.125 0.125 )))
(local.set $expected ( v128.const f32x4 0.125 0.125 0.125 0.125 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "max-abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -1.125 -1.125 -1.125 -1.125 ) ( v128.const f32x4 0.125 0.125 0.125 0.125 )))
(local.set $expected ( v128.const f32x4 1.125 1.125 1.125 1.125 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "min-abs" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f32x4 -1.125 -1.125 -1.125 -1.125 ) ( v128.const f32x4 0.125 0.125 0.125 0.125 )))
(local.set $expected ( v128.const f32x4 0.125 0.125 0.125 0.125 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)

