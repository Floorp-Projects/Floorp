var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "f64x2.min" ) ( param v128 v128 ) ( result v128 ) ( f64x2.min ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "f64x2.max" ) ( param v128 v128 ) ( result v128 ) ( f64x2.max ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "f64x2.abs" ) ( param v128 ) ( result v128 ) ( f64x2.abs ( local.get 0 ) ) ) ( func ( export "f64x2.min_with_const_0" ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0 1 ) ( v128.const f64x2 0 2 ) ) ) ( func ( export "f64x2.min_with_const_1" ) ( result v128 ) ( f64x2.min ( v128.const f64x2 2 -3 ) ( v128.const f64x2 1 3 ) ) ) ( func ( export "f64x2.min_with_const_2" ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0 1 ) ( v128.const f64x2 0 1 ) ) ) ( func ( export "f64x2.min_with_const_3" ) ( result v128 ) ( f64x2.min ( v128.const f64x2 2 3 ) ( v128.const f64x2 2 3 ) ) ) ( func ( export "f64x2.min_with_const_4" ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0x00 0x01 ) ( v128.const f64x2 0x00 0x02 ) ) ) ( func ( export "f64x2.min_with_const_5" ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0x02 0x80000000 ) ( v128.const f64x2 0x01 2147483648 ) ) ) ( func ( export "f64x2.min_with_const_6" ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0x00 0x01 ) ( v128.const f64x2 0x00 0x01 ) ) ) ( func ( export "f64x2.min_with_const_7" ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0x02 0x80000000 ) ( v128.const f64x2 0x02 0x80000000 ) ) ) ( func ( export "f64x2.min_with_const_9" ) ( param v128 ) ( result v128 ) ( f64x2.min ( local.get 0 ) ( v128.const f64x2 0 1 ) ) ) ( func ( export "f64x2.min_with_const_10" ) ( param v128 ) ( result v128 ) ( f64x2.min ( v128.const f64x2 2 -3 ) ( local.get 0 ) ) ) ( func ( export "f64x2.min_with_const_11" ) ( param v128 ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0 1 ) ( local.get 0 ) ) ) ( func ( export "f64x2.min_with_const_12" ) ( param v128 ) ( result v128 ) ( f64x2.min ( local.get 0 ) ( v128.const f64x2 2 3 ) ) ) ( func ( export "f64x2.min_with_const_13" ) ( param v128 ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0x00 0x01 ) ( local.get 0 ) ) ) ( func ( export "f64x2.min_with_const_14" ) ( param v128 ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0x02 0x80000000 ) ( local.get 0 ) ) ) ( func ( export "f64x2.min_with_const_15" ) ( param v128 ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0x00 0x01 ) ( local.get 0 ) ) ) ( func ( export "f64x2.min_with_const_16" ) ( param v128 ) ( result v128 ) ( f64x2.min ( v128.const f64x2 0x02 0x80000000 ) ( local.get 0 ) ) ) ( func ( export "f64x2.max_with_const_18" ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0 1 ) ( v128.const f64x2 0 2 ) ) ) ( func ( export "f64x2.max_with_const_19" ) ( result v128 ) ( f64x2.max ( v128.const f64x2 2 -3 ) ( v128.const f64x2 1 3 ) ) ) ( func ( export "f64x2.max_with_const_20" ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0 1 ) ( v128.const f64x2 0 1 ) ) ) ( func ( export "f64x2.max_with_const_21" ) ( result v128 ) ( f64x2.max ( v128.const f64x2 2 3 ) ( v128.const f64x2 2 3 ) ) ) ( func ( export "f64x2.max_with_const_22" ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0x00 0x01 ) ( v128.const f64x2 0x00 0x02 ) ) ) ( func ( export "f64x2.max_with_const_23" ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0x02 0x80000000 ) ( v128.const f64x2 0x01 2147483648 ) ) ) ( func ( export "f64x2.max_with_const_24" ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0x00 0x01 ) ( v128.const f64x2 0x00 0x01 ) ) ) ( func ( export "f64x2.max_with_const_25" ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0x02 0x80000000 ) ( v128.const f64x2 0x02 0x80000000 ) ) ) ( func ( export "f64x2.max_with_const_27" ) ( param v128 ) ( result v128 ) ( f64x2.max ( local.get 0 ) ( v128.const f64x2 0 1 ) ) ) ( func ( export "f64x2.max_with_const_28" ) ( param v128 ) ( result v128 ) ( f64x2.max ( v128.const f64x2 2 -3 ) ( local.get 0 ) ) ) ( func ( export "f64x2.max_with_const_29" ) ( param v128 ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0 1 ) ( local.get 0 ) ) ) ( func ( export "f64x2.max_with_const_30" ) ( param v128 ) ( result v128 ) ( f64x2.max ( local.get 0 ) ( v128.const f64x2 2 3 ) ) ) ( func ( export "f64x2.max_with_const_31" ) ( param v128 ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0x00 0x01 ) ( local.get 0 ) ) ) ( func ( export "f64x2.max_with_const_32" ) ( param v128 ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0x02 0x80000000 ) ( local.get 0 ) ) ) ( func ( export "f64x2.max_with_const_33" ) ( param v128 ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0x00 0x01 ) ( local.get 0 ) ) ) ( func ( export "f64x2.max_with_const_34" ) ( param v128 ) ( result v128 ) ( f64x2.max ( v128.const f64x2 0x02 0x80000000 ) ( local.get 0 ) ) ) ( func ( export "f64x2.abs_with_const_35" ) ( result v128 ) ( f64x2.abs ( v128.const f64x2 -0 -1 ) ) ) ( func ( export "f64x2.abs_with_const_36" ) ( result v128 ) ( f64x2.abs ( v128.const f64x2 -2 -3 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_0" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0 1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_1" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 1 -3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_2" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0 1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_3" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 2 3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_4" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0x00 0x01 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_5" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0x01 0x80000000 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_6" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0x00 0x01 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_7" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0x02 0x80000000 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_9" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 2 )))
(local.set $expected ( v128.const f64x2 0 1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_10" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 1 3 )))
(local.set $expected ( v128.const f64x2 1 -3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_11" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 1 )))
(local.set $expected ( v128.const f64x2 0 1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_12" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 2 3 )))
(local.set $expected ( v128.const f64x2 2 3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_13" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x00 0x02 )))
(local.set $expected ( v128.const f64x2 0x00 0x01 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_14" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x01 2147483648 )))
(local.set $expected ( v128.const f64x2 0x01 0x80000000 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_15" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x00 0x01 )))
(local.set $expected ( v128.const f64x2 0x00 0x01 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min_with_const_16" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x02 0x80000000 )))
(local.set $expected ( v128.const f64x2 0x02 0x80000000 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_18" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0 2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_19" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 2 3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_20" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0 1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_21" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 2 3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_22" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0x00 0x02 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_23" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0x02 2147483648 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_24" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0x00 0x01 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_25" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0x02 0x80000000 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_27" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 2 )))
(local.set $expected ( v128.const f64x2 0 2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_28" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 1 3 )))
(local.set $expected ( v128.const f64x2 2 3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_29" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 1 )))
(local.set $expected ( v128.const f64x2 0 1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_30" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 2 3 )))
(local.set $expected ( v128.const f64x2 2 3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_31" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x00 0x02 )))
(local.set $expected ( v128.const f64x2 0x00 0x02 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_32" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x01 2147483648 )))
(local.set $expected ( v128.const f64x2 0x02 2147483648 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_33" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x00 0x01 )))
(local.set $expected ( v128.const f64x2 0x00 0x01 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max_with_const_34" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x02 0x80000000 )))
(local.set $expected ( v128.const f64x2 0x02 0x80000000 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs_with_const_35" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 0 1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs_with_const_36" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const f64x2 2 3 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan 0 ) ( v128.const f64x2 0 1 )))
(local.set $expected ( v128.const f64x2 nan 0 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 1 ) ( v128.const f64x2 -nan 0 )))
(local.set $expected ( v128.const f64x2 -nan 0 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 1 ) ( v128.const f64x2 -nan 1 )))
(local.set $expected ( v128.const f64x2 nan 1 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan 0 ) ( v128.const f64x2 0 1 )))
(local.set $expected ( v128.const f64x2 nan 1 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 1 ) ( v128.const f64x2 -nan 0 )))
(local.set $expected ( v128.const f64x2 nan 1 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 1 ) ( v128.const f64x2 -nan 1 )))
(local.set $expected ( v128.const f64x2 nan 1 ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0xFFFFFFFF 0xFFFFFFFF)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0p+0 -0x0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0p+0 -0x0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0p+0 0x0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0p+0 0x0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 -0x0.0p+0 -0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1022 -0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p-1 -0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1.0000000000000p+0 -0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 nan nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -nan -nan ) ( v128.const f64x2 -nan -nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ) ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 0 ) ( v128.const f64x2 +0 -0 )))
(local.set $expected ( v128.const f64x2 0 -0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0 +0 ) ( v128.const f64x2 +0 -0 )))
(local.set $expected ( v128.const f64x2 -0 -0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.min" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0 -0 ) ( v128.const f64x2 +0 +0 )))
(local.set $expected ( v128.const f64x2 -0 -0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0 0 ) ( v128.const f64x2 +0 -0 )))
(local.set $expected ( v128.const f64x2 0 0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0 +0 ) ( v128.const f64x2 +0 -0 )))
(local.set $expected ( v128.const f64x2 0 0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.max" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0 -0 ) ( v128.const f64x2 +0 +0 )))
(local.set $expected ( v128.const f64x2 +0 +0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x0p+0 0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x0p+0 -0x0p+0 )))
(local.set $expected ( v128.const f64x2 0x0.0p+0 0x0.0p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1074 0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1074 -0x1p-1074 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1022 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1022 -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1022 0x1.0000000000000p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p-1 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p-1 -0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p-1 0x1.0000000000000p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1p+0 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1p+0 -0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1.0000000000000p+0 0x1.0000000000000p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 inf inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e038 01234567890123456789e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 )))
(local.set $expected ( v128.const f64x2 01234567890123456789e-038 01234567890123456789e-038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e038 0123456789.e038 )))
(local.set $expected ( v128.const f64x2 0123456789.e038 0123456789.e038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 0123456789.e+038 0123456789.e+038 )))
(local.set $expected ( v128.const f64x2 0123456789.e+038 0123456789.e+038 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.abs" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 -01234567890123456789.01234567890123456789 -01234567890123456789.01234567890123456789 )))
(local.set $expected ( v128.const f64x2 01234567890123456789.01234567890123456789 01234567890123456789.01234567890123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f64x2.abs ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f64x2.min ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( f64x2.max ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.abs-arg-empty ( result v128 ) ( f64x2.abs ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.min-1st-arg-empty ( result v128 ) ( f64x2.min ( v128.const f64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.min-arg-empty ( result v128 ) ( f64x2.min ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.max-1st-arg-empty ( result v128 ) ( f64x2.max ( v128.const f64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.max-arg-empty ( result v128 ) ( f64x2.max ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "max-min" ) ( param v128 v128 v128 ) ( result v128 ) ( f64x2.max ( f64x2.min ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "min-max" ) ( param v128 v128 v128 ) ( result v128 ) ( f64x2.min ( f64x2.max ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "max-abs" ) ( param v128 v128 ) ( result v128 ) ( f64x2.max ( f64x2.abs ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "min-abs" ) ( param v128 v128 ) ( result v128 ) ( f64x2.min ( f64x2.abs ( local.get 0 ) ) ( local.get 1 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "max-min" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const f64x2 1.125 1.125 ) ( v128.const f64x2 0.25 0.25 ) ( v128.const f64x2 0.125 0.125 )))
(local.set $expected ( v128.const f64x2 0.25 0.25 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const f64x2 1.125 1.125 ) ( v128.const f64x2 0.25 0.25 ) ( v128.const f64x2 0.125 0.125 )))
(local.set $expected ( v128.const f64x2 0.125 0.125 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const f64x2 -1.125 -1.125 ) ( v128.const f64x2 0.125 0.125 )))
(local.set $expected ( v128.const f64x2 1.125 1.125 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const f64x2 -1.125 -1.125 ) ( v128.const f64x2 0.125 0.125 )))
(local.set $expected ( v128.const f64x2 0.125 0.125 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)

