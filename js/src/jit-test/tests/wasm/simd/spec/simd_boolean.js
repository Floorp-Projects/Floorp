var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i8x16.any_true" ) ( param $0 v128 ) ( result i32 ) ( i8x16.any_true ( local.get $0 ) ) ) ( func ( export "i8x16.all_true" ) ( param $0 v128 ) ( result i32 ) ( i8x16.all_true ( local.get $0 ) ) ) ( func ( export "i16x8.any_true" ) ( param $0 v128 ) ( result i32 ) ( i16x8.any_true ( local.get $0 ) ) ) ( func ( export "i16x8.all_true" ) ( param $0 v128 ) ( result i32 ) ( i16x8.all_true ( local.get $0 ) ) ) ( func ( export "i32x4.any_true" ) ( param $0 v128 ) ( result i32 ) ( i32x4.any_true ( local.get $0 ) ) ) ( func ( export "i32x4.all_true" ) ( param $0 v128 ) ( result i32 ) ( i32x4.all_true ( local.get $0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
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
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 0 1 2 3 4 5 6 7 8 9 0xA 0xB 0xC 0xD 0xF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
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
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 0 1 2 3 4 5 6 7 8 9 0xA 0xB 0xC 0xD 0xF )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 0 1 2 0xB 0xC 0xD 0xF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 0 1 2 0xB 0xC 0xD 0xF )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 0 1 0xF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0x00 0x00 0x00 0x00 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xAB 0xAB 0xAB 0xAB )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 01_234_567_890 01_234_567_890 01_234_567_890 01_234_567_890 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.any_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 0 1 0xF )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0x00 0x00 0x00 0x00 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xAB 0xAB 0xAB 0xAB )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 01_234_567_890 01_234_567_890 01_234_567_890 01_234_567_890 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.all_true" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "i8x16_any_true_as_if_cond" ) ( param v128 ) ( result i32 ) ( if ( result i32 ) ( i8x16.any_true ( local.get 0 ) ) ( then ( i32.const 1 ) ) ( else ( i32.const 0 ) ) ) ) ( func ( export "i16x8_any_true_as_if_cond" ) ( param v128 ) ( result i32 ) ( if ( result i32 ) ( i16x8.any_true ( local.get 0 ) ) ( then ( i32.const 1 ) ) ( else ( i32.const 0 ) ) ) ) ( func ( export "i32x4_any_true_as_if_cond" ) ( param v128 ) ( result i32 ) ( if ( result i32 ) ( i32x4.any_true ( local.get 0 ) ) ( then ( i32.const 1 ) ) ( else ( i32.const 0 ) ) ) ) ( func ( export "i8x16_all_true_as_if_cond" ) ( param v128 ) ( result i32 ) ( if ( result i32 ) ( i8x16.all_true ( local.get 0 ) ) ( then ( i32.const 1 ) ) ( else ( i32.const 0 ) ) ) ) ( func ( export "i16x8_all_true_as_if_cond" ) ( param v128 ) ( result i32 ) ( if ( result i32 ) ( i16x8.all_true ( local.get 0 ) ) ( then ( i32.const 1 ) ) ( else ( i32.const 0 ) ) ) ) ( func ( export "i32x4_all_true_as_if_cond" ) ( param v128 ) ( result i32 ) ( if ( result i32 ) ( i32x4.all_true ( local.get 0 ) ) ( then ( i32.const 1 ) ) ( else ( i32.const 0 ) ) ) ) ( func ( export "i8x16_any_true_as_select_cond" ) ( param v128 ) ( result i32 ) ( select ( i32.const 1 ) ( i32.const 0 ) ( i8x16.any_true ( local.get 0 ) ) ) ) ( func ( export "i16x8_any_true_as_select_cond" ) ( param v128 ) ( result i32 ) ( select ( i32.const 1 ) ( i32.const 0 ) ( i16x8.any_true ( local.get 0 ) ) ) ) ( func ( export "i32x4_any_true_as_select_cond" ) ( param v128 ) ( result i32 ) ( select ( i32.const 1 ) ( i32.const 0 ) ( i32x4.any_true ( local.get 0 ) ) ) ) ( func ( export "i8x16_all_true_as_select_cond" ) ( param v128 ) ( result i32 ) ( select ( i32.const 1 ) ( i32.const 0 ) ( i8x16.all_true ( local.get 0 ) ) ) ) ( func ( export "i16x8_all_true_as_select_cond" ) ( param v128 ) ( result i32 ) ( select ( i32.const 1 ) ( i32.const 0 ) ( i16x8.all_true ( local.get 0 ) ) ) ) ( func ( export "i32x4_all_true_as_select_cond" ) ( param v128 ) ( result i32 ) ( select ( i32.const 1 ) ( i32.const 0 ) ( i32x4.all_true ( local.get 0 ) ) ) ) ( func ( export "i8x16_any_true_as_br_if_cond" ) ( param $0 v128 ) ( result i32 ) ( local $1 i32 ) ( local.set $1 ( i32.const 2 ) ) ( block ( local.set $1 ( i32.const 1 ) ) ( br_if 0 ( i8x16.any_true ( local.get $0 ) ) ) ( local.set $1 ( i32.const 0 ) ) ) ( local.get $1 ) ) ( func ( export "i16x8_any_true_as_br_if_cond" ) ( param $0 v128 ) ( result i32 ) ( local $1 i32 ) ( local.set $1 ( i32.const 2 ) ) ( block ( local.set $1 ( i32.const 1 ) ) ( br_if 0 ( i16x8.any_true ( local.get $0 ) ) ) ( local.set $1 ( i32.const 0 ) ) ) ( local.get $1 ) ) ( func ( export "i32x4_any_true_as_br_if_cond" ) ( param $0 v128 ) ( result i32 ) ( local $1 i32 ) ( local.set $1 ( i32.const 2 ) ) ( block ( local.set $1 ( i32.const 1 ) ) ( br_if 0 ( i32x4.any_true ( local.get $0 ) ) ) ( local.set $1 ( i32.const 0 ) ) ) ( local.get $1 ) ) ( func ( export "i8x16_all_true_as_br_if_cond" ) ( param $0 v128 ) ( result i32 ) ( local $1 i32 ) ( local.set $1 ( i32.const 2 ) ) ( block ( local.set $1 ( i32.const 1 ) ) ( br_if 0 ( i8x16.all_true ( local.get $0 ) ) ) ( local.set $1 ( i32.const 0 ) ) ) ( local.get $1 ) ) ( func ( export "i16x8_all_true_as_br_if_cond" ) ( param $0 v128 ) ( result i32 ) ( local $1 i32 ) ( local.set $1 ( i32.const 2 ) ) ( block ( local.set $1 ( i32.const 1 ) ) ( br_if 0 ( i16x8.all_true ( local.get $0 ) ) ) ( local.set $1 ( i32.const 0 ) ) ) ( local.get $1 ) ) ( func ( export "i32x4_all_true_as_br_if_cond" ) ( param $0 v128 ) ( result i32 ) ( local $1 i32 ) ( local.set $1 ( i32.const 2 ) ) ( block ( local.set $1 ( i32.const 1 ) ) ( br_if 0 ( i32x4.all_true ( local.get $0 ) ) ) ( local.set $1 ( i32.const 0 ) ) ) ( local.get $1 ) ) ( func ( export "i8x16_any_true_as_i32.and_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.and ( i8x16.any_true ( local.get $0 ) ) ( i8x16.any_true ( local.get $1 ) ) ) ) ( func ( export "i16x8_any_true_as_i32.and_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.and ( i16x8.any_true ( local.get $0 ) ) ( i16x8.any_true ( local.get $1 ) ) ) ) ( func ( export "i32x4_any_true_as_i32.and_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.and ( i32x4.any_true ( local.get $0 ) ) ( i32x4.any_true ( local.get $1 ) ) ) ) ( func ( export "i8x16_any_true_as_i32.or_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.or ( i8x16.any_true ( local.get $0 ) ) ( i8x16.any_true ( local.get $1 ) ) ) ) ( func ( export "i16x8_any_true_as_i32.or_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.or ( i16x8.any_true ( local.get $0 ) ) ( i16x8.any_true ( local.get $1 ) ) ) ) ( func ( export "i32x4_any_true_as_i32.or_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.or ( i32x4.any_true ( local.get $0 ) ) ( i32x4.any_true ( local.get $1 ) ) ) ) ( func ( export "i8x16_any_true_as_i32.xor_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.xor ( i8x16.any_true ( local.get $0 ) ) ( i8x16.any_true ( local.get $1 ) ) ) ) ( func ( export "i16x8_any_true_as_i32.xor_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.xor ( i16x8.any_true ( local.get $0 ) ) ( i16x8.any_true ( local.get $1 ) ) ) ) ( func ( export "i32x4_any_true_as_i32.xor_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.xor ( i32x4.any_true ( local.get $0 ) ) ( i32x4.any_true ( local.get $1 ) ) ) ) ( func ( export "i8x16_all_true_as_i32.and_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.and ( i8x16.all_true ( local.get $0 ) ) ( i8x16.all_true ( local.get $1 ) ) ) ) ( func ( export "i16x8_all_true_as_i32.and_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.and ( i16x8.all_true ( local.get $0 ) ) ( i16x8.all_true ( local.get $1 ) ) ) ) ( func ( export "i32x4_all_true_as_i32.and_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.and ( i32x4.all_true ( local.get $0 ) ) ( i32x4.all_true ( local.get $1 ) ) ) ) ( func ( export "i8x16_all_true_as_i32.or_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.or ( i8x16.all_true ( local.get $0 ) ) ( i8x16.all_true ( local.get $1 ) ) ) ) ( func ( export "i16x8_all_true_as_i32.or_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.or ( i16x8.all_true ( local.get $0 ) ) ( i16x8.all_true ( local.get $1 ) ) ) ) ( func ( export "i32x4_all_true_as_i32.or_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.or ( i32x4.all_true ( local.get $0 ) ) ( i32x4.all_true ( local.get $1 ) ) ) ) ( func ( export "i8x16_all_true_as_i32.xor_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.xor ( i8x16.all_true ( local.get $0 ) ) ( i8x16.all_true ( local.get $1 ) ) ) ) ( func ( export "i16x8_all_true_as_i32.xor_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.xor ( i16x8.all_true ( local.get $0 ) ) ( i16x8.all_true ( local.get $1 ) ) ) ) ( func ( export "i32x4_all_true_as_i32.xor_operand" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32.xor ( i32x4.all_true ( local.get $0 ) ) ( i32x4.all_true ( local.get $1 ) ) ) ) ( func ( export "i8x16_any_true_with_v128.not" ) ( param $0 v128 ) ( result i32 ) ( i8x16.any_true ( v128.not ( local.get $0 ) ) ) ) ( func ( export "i16x8_any_true_with_v128.not" ) ( param $0 v128 ) ( result i32 ) ( i16x8.any_true ( v128.not ( local.get $0 ) ) ) ) ( func ( export "i32x4_any_true_with_v128.not" ) ( param $0 v128 ) ( result i32 ) ( i32x4.any_true ( v128.not ( local.get $0 ) ) ) ) ( func ( export "i8x16_any_true_with_v128.and" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i8x16.any_true ( v128.and ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i16x8_any_true_with_v128.and" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i16x8.any_true ( v128.and ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i32x4_any_true_with_v128.and" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32x4.any_true ( v128.and ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i8x16_any_true_with_v128.or" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i8x16.any_true ( v128.or ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i16x8_any_true_with_v128.or" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i16x8.any_true ( v128.or ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i32x4_any_true_with_v128.or" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32x4.any_true ( v128.or ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i8x16_any_true_with_v128.xor" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i8x16.any_true ( v128.xor ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i16x8_any_true_with_v128.xor" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i16x8.any_true ( v128.xor ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i32x4_any_true_with_v128.xor" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32x4.any_true ( v128.xor ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i8x16_any_true_with_v128.bitselect" ) ( param $0 v128 ) ( param $1 v128 ) ( param $2 v128 ) ( result i32 ) ( i8x16.any_true ( v128.bitselect ( local.get $0 ) ( local.get $1 ) ( local.get $2 ) ) ) ) ( func ( export "i16x8_any_true_with_v128.bitselect" ) ( param $0 v128 ) ( param $1 v128 ) ( param $2 v128 ) ( result i32 ) ( i16x8.any_true ( v128.bitselect ( local.get $0 ) ( local.get $1 ) ( local.get $2 ) ) ) ) ( func ( export "i32x4_any_true_with_v128.bitselect" ) ( param $0 v128 ) ( param $1 v128 ) ( param $2 v128 ) ( result i32 ) ( i32x4.any_true ( v128.bitselect ( local.get $0 ) ( local.get $1 ) ( local.get $2 ) ) ) ) ( func ( export "i8x16_all_true_with_v128.not" ) ( param $0 v128 ) ( result i32 ) ( i8x16.all_true ( v128.not ( local.get $0 ) ) ) ) ( func ( export "i16x8_all_true_with_v128.not" ) ( param $0 v128 ) ( result i32 ) ( i16x8.all_true ( v128.not ( local.get $0 ) ) ) ) ( func ( export "i32x4_all_true_with_v128.not" ) ( param $0 v128 ) ( result i32 ) ( i32x4.all_true ( v128.not ( local.get $0 ) ) ) ) ( func ( export "i8x16_all_true_with_v128.and" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i8x16.all_true ( v128.and ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i16x8_all_true_with_v128.and" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i16x8.all_true ( v128.and ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i32x4_all_true_with_v128.and" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32x4.all_true ( v128.and ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i8x16_all_true_with_v128.or" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i8x16.all_true ( v128.or ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i16x8_all_true_with_v128.or" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i16x8.all_true ( v128.or ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i32x4_all_true_with_v128.or" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32x4.all_true ( v128.or ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i8x16_all_true_with_v128.xor" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i8x16.all_true ( v128.xor ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i16x8_all_true_with_v128.xor" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i16x8.all_true ( v128.xor ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i32x4_all_true_with_v128.xor" ) ( param $0 v128 ) ( param $1 v128 ) ( result i32 ) ( i32x4.all_true ( v128.xor ( local.get $0 ) ( local.get $1 ) ) ) ) ( func ( export "i8x16_all_true_with_v128.bitselect" ) ( param $0 v128 ) ( param $1 v128 ) ( param $2 v128 ) ( result i32 ) ( i8x16.all_true ( v128.bitselect ( local.get $0 ) ( local.get $1 ) ( local.get $2 ) ) ) ) ( func ( export "i16x8_all_true_with_v128.bitselect" ) ( param $0 v128 ) ( param $1 v128 ) ( param $2 v128 ) ( result i32 ) ( i16x8.all_true ( v128.bitselect ( local.get $0 ) ( local.get $1 ) ( local.get $2 ) ) ) ) ( func ( export "i32x4_all_true_with_v128.bitselect" ) ( param $0 v128 ) ( param $1 v128 ) ( param $2 v128 ) ( result i32 ) ( i32x4.all_true ( v128.bitselect ( local.get $0 ) ( local.get $1 ) ( local.get $2 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_if_cond" (func $f (param v128) (result i32)))
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
  (import "" "i8x16_any_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 1 0 0 0 1 0 0 0 1 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 1 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_if_cond" (func $f (param v128) (result i32)))
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
  (import "" "i8x16_all_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 0 1 1 1 0 1 1 1 0 1 1 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 0 1 1 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_select_cond" (func $f (param v128) (result i32)))
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
  (import "" "i8x16_any_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_select_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_br_if_cond" (func $f (param v128) (result i32)))
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
  (import "" "i8x16_any_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_br_if_cond" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 1 0 ) ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 ) ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 1 0 ) ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 ) ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 1 0 ) ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 1 0 ) ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 0 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.and_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.or_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 1 1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 1 1 1 1 ) ( v128.const i32x4 1 1 0 1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_as_i32.xor_operand" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 -1 0 ) ( v128.const i16x8 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 0 ) ( v128.const i32x4 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 -1 0 ) ( v128.const i16x8 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 0 ) ( v128.const i32x4 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_any_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0xFF 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA ) ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_any_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA ) ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0xFF 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xAA 0xAA 0xAA 0xAA ) ( v128.const i32x4 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_any_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xAA 0xAA 0xAA 0xAA ) ( v128.const i32x4 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0x55 0x55 0xFF 0x55 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.not" (func $f (param v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 -1 0 ) ( v128.const i16x8 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.and" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 0 ) ( v128.const i32x4 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 -1 0 ) ( v128.const i16x8 0 0 0 0 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.or" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 -1 0 ) ( v128.const i32x4 0 0 -1 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0 -1 0 -1 0 -1 0 -1 0 -1 0 -1 0 -1 0 -1 ) ( v128.const i8x16 -1 0 -1 0 -1 0 -1 0 -1 0 -1 0 -1 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0 -1 0 -1 0 -1 0 -1 ) ( v128.const i16x8 -1 0 -1 0 -1 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 -1 -1 -1 -1 ) ( v128.const i32x4 -1 -1 -1 -1 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.xor" (func $f (param v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0 -1 0 -1 ) ( v128.const i32x4 -1 0 -1 0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16_all_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA ) ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8_all_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i16x8 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA ) ( v128.const i16x8 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xAA 0xAA 0xAA 0xAA ) ( v128.const i32x4 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0x55 0x55 0x55 0x55 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4_all_true_with_v128.bitselect" (func $f (param v128 v128 v128) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( v128.const i32x4 0xAA 0xAA 0xAA 0xAA ) ( v128.const i32x4 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.any_true ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i8x16.all_true ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.any_true ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i16x8.all_true ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i32x4.any_true ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result i32 ) ( i32x4.all_true ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result i32) (f32x4.any_true (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result i32) (f32x4.all_true (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result i32) (f64x2.any_true (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result i32) (f64x2.all_true (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.any_true-arg-empty ( result v128 ) ( i8x16.any_true ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.all_true-arg-empty ( result v128 ) ( i8x16.all_true ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.any_true-arg-empty ( result v128 ) ( i16x8.any_true ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.all_true-arg-empty ( result v128 ) ( i16x8.all_true ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.any_true-arg-empty ( result v128 ) ( i32x4.any_true ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.all_true-arg-empty ( result v128 ) ( i32x4.all_true ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)

