var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i64x2.add" ) ( param v128 v128 ) ( result v128 ) ( i64x2.add ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i64x2.sub" ) ( param v128 v128 ) ( result v128 ) ( i64x2.sub ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i64x2.mul" ) ( param v128 v128 ) ( result v128 ) ( i64x2.mul ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i64x2.neg" ) ( param v128 ) ( result v128 ) ( i64x2.neg ( local.get 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 0 0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -1 -1 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -2 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 4611686018427387903 4611686018427387903 ) ( v128.const i64x2 4611686018427387904 4611686018427387904 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 4611686018427387904 4611686018427387904 ) ( v128.const i64x2 4611686018427387904 4611686018427387904 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387903 -4611686018427387903 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387904 -4611686018427387904 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387905 -4611686018427387905 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775805 9223372036854775805 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 9223372036854775806 9223372036854775806 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775806 9223372036854775806 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775808 9223372036854775808 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775806 -9223372036854775806 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775807 9223372036854775807 ) ( v128.const i64x2 9223372036854775807 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 -2 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -9223372036854775808 -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -9223372036854775807 -9223372036854775807 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 0 0 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -2 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 9223372036854775807 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 9223372036854775806 9223372036854775806 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 -9223372036854775808 -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 18446744073709551615 18446744073709551615 )))
(local.set $expected ( v128.const i64x2 -2 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x3fffffffffffffff 0x3fffffffffffffff ) ( v128.const i64x2 0x4000000000000000 0x4000000000000000 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x4000000000000000 0x4000000000000000 ) ( v128.const i64x2 0x4000000000000000 0x4000000000000000 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x3fffffffffffffff -0x3fffffffffffffff ) ( v128.const i64x2 -0x40000000fffffff -0x40000000fffffff )))
(local.set $expected ( v128.const i64x2 -4899916394847535102 -4899916394847535102 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x4000000000000000 -0x4000000000000000 ) ( v128.const i64x2 -0x400000000000000 -0x400000000000000 )))
(local.set $expected ( v128.const i64x2 -4899916394579099648 -4899916394579099648 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x4000000000000000 -0x4000000000000000 ) ( v128.const i64x2 -0x400000000000001 -0x400000000000001 )))
(local.set $expected ( v128.const i64x2 -4899916394579099649 -4899916394579099649 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x7ffffffffffffff 0x7ffffffffffffff )))
(local.set $expected ( v128.const i64x2 -8646911284551352322 -8646911284551352322 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x01 0x01 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i64x2 -0x01 -0x01 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x8000000000000000 0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i64x2 0x8000000000000000 0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i64x2 0x01 0x01 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 -2 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i8x16 0 0 0 0 0 0 0 0x80 0 0 0 0 0 0 0 0x80 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i16x8 0 0 0 0x8000 0 0 0 0x8000 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i32x4 0 0x80000000 0 0x80000000 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const f64x2 +0.0 +0.0 )))
(local.set $expected ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const f64x2 -0.0 -0.0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const f64x2 1.0 1.0 )))
(local.set $expected ( v128.const i64x2 0xbff0000000000000 0xbff0000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const f64x2 -1.0 -1.0 )))
(local.set $expected ( v128.const i64x2 0x3ff0000000000000 0x3ff0000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const f64x2 +inf +inf )))
(local.set $expected ( v128.const i64x2 0x7ff0000000000001 0x7ff0000000000001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const i64x2 0xfff0000000000001 0xfff0000000000001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const i64x2 0x7ff8000000000001 0x7ff8000000000001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 2 )))
(local.set $expected ( v128.const i64x2 0 3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 ) ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 )))
(local.set $expected ( v128.const i64x2 02_469_135_780_246_913_578 02_469_135_780_246_913_578 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x0_1234_5678_90AB_cdef 0x0_1234_5678_90AB_cdef ) ( v128.const i64x2 0x0_90AB_cdef_1234_5678 0x0_90AB_cdef_1234_5678 )))
(local.set $expected ( v128.const i64x2 0x0_a2e0_2467_a2e0_2467 0x0_a2e0_2467_a2e0_2467 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 0 0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -1 -1 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 4611686018427387903 4611686018427387903 ) ( v128.const i64x2 4611686018427387904 4611686018427387904 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 4611686018427387904 4611686018427387904 ) ( v128.const i64x2 4611686018427387904 4611686018427387904 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387903 -4611686018427387903 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387904 -4611686018427387904 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387905 -4611686018427387905 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775805 9223372036854775805 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 9223372036854775804 9223372036854775804 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775806 9223372036854775806 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 9223372036854775805 9223372036854775805 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775808 9223372036854775808 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775806 -9223372036854775806 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -9223372036854775805 -9223372036854775805 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -9223372036854775806 -9223372036854775806 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775807 9223372036854775807 ) ( v128.const i64x2 9223372036854775807 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -9223372036854775808 -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -9223372036854775807 -9223372036854775807 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 0 0 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 -2 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 9223372036854775807 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 -9223372036854775808 -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 18446744073709551615 18446744073709551615 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x3fffffffffffffff 0x3fffffffffffffff ) ( v128.const i64x2 0x4000000000000000 0x4000000000000000 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x4000000000000000 0x4000000000000000 ) ( v128.const i64x2 0x4000000000000000 0x4000000000000000 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x3fffffffffffffff -0x3fffffffffffffff ) ( v128.const i64x2 -0x40000000fffffff -0x40000000fffffff )))
(local.set $expected ( v128.const i64x2 -4323455642007240704 -4323455642007240704 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x4000000000000000 -0x4000000000000000 ) ( v128.const i64x2 -0x400000000000000 -0x400000000000000 )))
(local.set $expected ( v128.const i64x2 -4323455642275676160 -4323455642275676160 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x4000000000000000 -0x4000000000000000 ) ( v128.const i64x2 -0x400000000000001 -0x400000000000001 )))
(local.set $expected ( v128.const i64x2 -4323455642275676159 -4323455642275676159 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x7ffffffffffffff 0x7ffffffffffffff )))
(local.set $expected ( v128.const i64x2 8646911284551352320 8646911284551352320 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x01 0x01 )))
(local.set $expected ( v128.const i64x2 9223372036854775806 9223372036854775806 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i64x2 -0x01 -0x01 )))
(local.set $expected ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x8000000000000000 0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i64x2 0x8000000000000000 0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i64x2 0x01 0x01 )))
(local.set $expected ( v128.const i64x2 -2 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i8x16 0 0 0 0 0 0 0 0x80 0 0 0 0 0 0 0 0x80 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i64x2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i16x8 0 0 0 0x8000 0 0 0 0x8000 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i64x2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i32x4 0 0x80000000 0 0x80000000 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i64x2 2 2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const f64x2 +0.0 +0.0 )))
(local.set $expected ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const f64x2 -0.0 -0.0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const f64x2 1.0 1.0 )))
(local.set $expected ( v128.const i64x2 0x4010000000000000 0x4010000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const f64x2 -1.0 -1.0 )))
(local.set $expected ( v128.const i64x2 0xc010000000000000 0xc010000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x1 0x1 ) ( v128.const f64x2 +inf +inf )))
(local.set $expected ( v128.const i64x2 0x8010000000000001 0x8010000000000001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x1 0x1 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const i64x2 0x0010000000000001 0x0010000000000001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x1 0x1 ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const i64x2 0x8008000000000001 0x8008000000000001 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 0 0x02 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 2 )))
(local.set $expected ( v128.const i64x2 0 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 03_214_567_890_123_456_789 03_214_567_890_123_456_789 ) ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 )))
(local.set $expected ( v128.const i64x2 01_980_000_000_000_000_000 01_980_000_000_000_000_000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x0_90AB_cdef_8765_4321 0x0_90AB_cdef_8765_4321 ) ( v128.const i64x2 0x0_1234_5678_90AB_cdef 0x0_1234_5678_90AB_cdef )))
(local.set $expected ( v128.const i64x2 0x0_7e77_7776_f6b9_7532 0x0_7e77_7776_f6b9_7532 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 0 0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -1 -1 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 4611686018427387903 4611686018427387903 ) ( v128.const i64x2 4611686018427387904 4611686018427387904 )))
(local.set $expected ( v128.const i64x2 -4611686018427387904 -4611686018427387904 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 4611686018427387904 4611686018427387904 ) ( v128.const i64x2 4611686018427387904 4611686018427387904 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387903 -4611686018427387903 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 -4611686018427387904 -4611686018427387904 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387904 -4611686018427387904 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -4611686018427387905 -4611686018427387905 ) ( v128.const i64x2 -4611686018427387904 -4611686018427387904 )))
(local.set $expected ( v128.const i64x2 4611686018427387904 4611686018427387904 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775805 9223372036854775805 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 9223372036854775805 9223372036854775805 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775806 9223372036854775806 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 9223372036854775806 9223372036854775806 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775808 9223372036854775808 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775806 -9223372036854775806 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 9223372036854775806 9223372036854775806 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775807 9223372036854775807 ) ( v128.const i64x2 9223372036854775807 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -9223372036854775808 -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ) ( v128.const i64x2 -9223372036854775807 -9223372036854775807 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 0 0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 9223372036854775807 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 -9223372036854775808 -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 ) ( v128.const i64x2 18446744073709551615 18446744073709551615 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x3fffffffffffffff 0x3fffffffffffffff ) ( v128.const i64x2 0x4000000000000000 0x4000000000000000 )))
(local.set $expected ( v128.const i64x2 -4611686018427387904 -4611686018427387904 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x4000000000000000 0x4000000000000000 ) ( v128.const i64x2 0x4000000000000000 0x4000000000000000 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x3fffffffffffffff -0x3fffffffffffffff ) ( v128.const i64x2 -0x40000000fffffff -0x40000000fffffff )))
(local.set $expected ( v128.const i64x2 -4899916394847535103 -4899916394847535103 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x4000000000000000 -0x4000000000000000 ) ( v128.const i64x2 -0x400000000000000 -0x400000000000000 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x4000000000000000 -0x4000000000000000 ) ( v128.const i64x2 -0x400000000000001 -0x400000000000001 )))
(local.set $expected ( v128.const i64x2 4611686018427387904 4611686018427387904 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x7ffffffffffffff 0x7ffffffffffffff )))
(local.set $expected ( v128.const i64x2 8646911284551352321 8646911284551352321 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x01 0x01 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i64x2 -0x01 -0x01 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ) ( v128.const i64x2 0x8000000000000000 0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i64x2 0x8000000000000000 0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i64x2 0x01 0x01 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i8x16 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 0x2 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i16x8 0 0 0 0x02 0 0 0 0x02 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ) ( v128.const i32x4 0 0x02 0 0x02 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x80000000 0x80000000 ) ( v128.const f64x2 +0.0 +0.0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x80000000 0x80000000 ) ( v128.const f64x2 -0.0 -0.0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x80000000 0x80000000 ) ( v128.const f64x2 1.0 1.0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x80000000 0x80000000 ) ( v128.const f64x2 -1.0 -1.0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x1 0x1 ) ( v128.const f64x2 +inf +inf )))
(local.set $expected ( v128.const i64x2 0x7ff0000000000000 0x7ff0000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x1 0x1 ) ( v128.const f64x2 -inf -inf )))
(local.set $expected ( v128.const i64x2 0xfff0000000000000 0xfff0000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x1 0x1 ) ( v128.const f64x2 nan nan )))
(local.set $expected ( v128.const i64x2 0x7ff8000000000000 0x7ff8000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 0 0xffffffffffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 2 )))
(local.set $expected ( v128.const i64x2 0 0x02 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 ) ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 )))
(local.set $expected ( v128.const i64x2 09_710_478_858_155_731_897 09_710_478_858_155_731_897 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x0_1234_5678_90AB_cdef 0x0_1234_5678_90AB_cdef ) ( v128.const i64x2 0x0_90AB_cdef_8765_4321 0x0_90AB_cdef_8765_4321 )))
(local.set $expected ( v128.const i64x2 0x0_602f_05e9_e556_18cf 0x0_602f_05e9_e556_18cf ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 1 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -1 -1 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775806 9223372036854775806 )))
(local.set $expected ( v128.const i64x2 -9223372036854775806 -9223372036854775806 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775807 -9223372036854775807 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 9223372036854775807 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 18446744073709551615 18446744073709551615 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x01 0x01 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x01 -0x01 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x8000000000000000 -0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -0x7fffffffffffffff -0x7fffffffffffffff )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff )))
(local.set $expected ( v128.const i64x2 -9223372036854775807 -9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x8000000000000000 0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xffffffffffffffff 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.neg ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.add ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.sub ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.mul ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.neg-arg-empty ( result v128 ) ( i64x2.neg ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.add-1st-arg-empty ( result v128 ) ( i64x2.add ( v128.const i64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.add-arg-empty ( result v128 ) ( i64x2.add ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.sub-1st-arg-empty ( result v128 ) ( i64x2.sub ( v128.const i64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.sub-arg-empty ( result v128 ) ( i64x2.sub ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.mul-1st-arg-empty ( result v128 ) ( i64x2.mul ( v128.const i64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.mul-arg-empty ( result v128 ) ( i64x2.mul ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "add-sub" ) ( param v128 v128 v128 ) ( result v128 ) ( i64x2.add ( i64x2.sub ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "mul-add" ) ( param v128 v128 v128 ) ( result v128 ) ( i64x2.mul ( i64x2.add ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "mul-sub" ) ( param v128 v128 v128 ) ( result v128 ) ( i64x2.mul ( i64x2.sub ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "sub-add" ) ( param v128 v128 v128 ) ( result v128 ) ( i64x2.sub ( i64x2.add ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "add-neg" ) ( param v128 v128 ) ( result v128 ) ( i64x2.add ( i64x2.neg ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "mul-neg" ) ( param v128 v128 ) ( result v128 ) ( i64x2.mul ( i64x2.neg ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "sub-neg" ) ( param v128 v128 ) ( result v128 ) ( i64x2.sub ( i64x2.neg ( local.get 0 ) ) ( local.get 1 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "add-sub" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 2 ) ( v128.const i64x2 0 2 )))
(local.set $expected ( v128.const i64x2 0 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "mul-add" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 1 ) ( v128.const i64x2 2 2 )))
(local.set $expected ( v128.const i64x2 0 4 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "mul-sub" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 2 ) ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 1 )))
(local.set $expected ( v128.const i64x2 0 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "sub-add" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 2 ) ( v128.const i64x2 0 2 )))
(local.set $expected ( v128.const i64x2 0 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "add-neg" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 1 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "mul-neg" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 2 2 )))
(local.set $expected ( v128.const i64x2 0 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "sub-neg" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 1 ) ( v128.const i64x2 0 1 )))
(local.set $expected ( v128.const i64x2 0 -2 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)

