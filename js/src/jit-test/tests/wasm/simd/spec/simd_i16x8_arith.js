var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i16x8.add" ) ( param v128 v128 ) ( result v128 ) ( i16x8.add ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i16x8.sub" ) ( param v128 v128 ) ( result v128 ) ( i16x8.sub ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i16x8.mul" ) ( param v128 v128 ) ( result v128 ) ( i16x8.mul ( local.get 0 ) ( local.get 1 ) ) ) ( func ( export "i16x8.neg" ) ( param v128 ) ( result v128 ) ( i16x8.neg ( local.get 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 16383 16383 16383 16383 16383 16383 16383 16383 ) ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 ) ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16383 -16383 -16383 -16383 -16383 -16383 -16383 -16383 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16385 -16385 -16385 -16385 -16385 -16385 -16385 -16385 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32765 32765 32765 32765 32765 32765 32765 32765 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32768 32768 32768 32768 32768 32768 32768 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32766 -32766 -32766 -32766 -32766 -32766 -32766 -32766 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ) ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 )))
(local.set $expected ( v128.const i16x8 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 )))
(local.set $expected ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff ) ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 ) ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff ) ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 ) ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 ) ( v128.const i16x8 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i16x8 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ) ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ) ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i8x16 0 0x80 0 0x80 0 0x80 0 0x80 0 0x80 0 0x80 0 0x80 0 0x80 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i32x4 0x80008000 0x80008000 0x80008000 0x80008000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 +0.0 +0.0 +0.0 +0.0 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 -0.0 -0.0 -0.0 -0.0 )))
(local.set $expected ( v128.const i16x8 0x8000 0 0x8000 0 0x8000 0 0x8000 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 1.0 1.0 1.0 1.0 )))
(local.set $expected ( v128.const i16x8 0x8000 0xbf80 0x8000 0xbf80 0x8000 0xbf80 0x8000 0xbf80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 -1.0 -1.0 -1.0 -1.0 )))
(local.set $expected ( v128.const i16x8 0x8000 0x3f80 0x8000 0x3f80 0x8000 0x3f80 0x8000 0x3f80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 +inf +inf +inf +inf )))
(local.set $expected ( v128.const i16x8 0x01 0x7f81 0x01 0x7f81 0x01 0x7f81 0x01 0x7f81 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const i16x8 0x01 0xff81 0x01 0xff81 0x01 0xff81 0x01 0xff81 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const i16x8 0x01 0x7fc1 0x01 0x7fc1 0x01 0x7fc1 0x01 0x7fc1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 0xffff 0xfffe 0xfffd 0xfffc 0xfffb 0xfffa 0xfff9 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 2 4 6 8 10 12 14 )))
(local.set $expected ( v128.const i16x8 0 3 6 9 12 15 18 21 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 )))
(local.set $expected ( v128.const i16x8 03_598 03_598 03_598 03_598 03_598 03_598 03_598 03_598 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.add" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 )))
(local.set $expected ( v128.const i16x8 0x0_68ac 0x0_68ac 0x0_68ac 0x0_68ac 0x0_68ac 0x0_68ac 0x0_68ac 0x0_68ac ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 16383 16383 16383 16383 16383 16383 16383 16383 ) ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 ) ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16383 -16383 -16383 -16383 -16383 -16383 -16383 -16383 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16385 -16385 -16385 -16385 -16385 -16385 -16385 -16385 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32765 32765 32765 32765 32765 32765 32765 32765 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 32764 32764 32764 32764 32764 32764 32764 32764 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 32765 32765 32765 32765 32765 32765 32765 32765 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32768 32768 32768 32768 32768 32768 32768 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32766 -32766 -32766 -32766 -32766 -32766 -32766 -32766 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -32765 -32765 -32765 -32765 -32765 -32765 -32765 -32765 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -32766 -32766 -32766 -32766 -32766 -32766 -32766 -32766 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ) ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff ) ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 ) ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff ) ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 ) ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 ) ( v128.const i16x8 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 )))
(local.set $expected ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i16x8 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ) ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 )))
(local.set $expected ( v128.const i16x8 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ) ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i8x16 0 0x80 0 0x80 0 0x80 0 0x80 0 0x80 0 0x80 0 0x80 0 0x80 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0x02 0x02 0x02 0x02 0x02 0x02 0x02 0x02 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i32x4 0x80008000 0x80008000 0x80008000 0x80008000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i16x8 0x02 0x02 0x02 0x02 0x02 0x02 0x02 0x02 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 +0.0 +0.0 +0.0 +0.0 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 -0.0 -0.0 -0.0 -0.0 )))
(local.set $expected ( v128.const i16x8 0x8000 0 0x8000 0 0x8000 0 0x8000 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 1.0 1.0 1.0 1.0 )))
(local.set $expected ( v128.const i16x8 0x8000 0x4080 0x8000 0x4080 0x8000 0x4080 0x8000 0x4080 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 -1.0 -1.0 -1.0 -1.0 )))
(local.set $expected ( v128.const i16x8 0x8000 0xc080 0x8000 0xc080 0x8000 0xc080 0x8000 0xc080 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 +inf +inf +inf +inf )))
(local.set $expected ( v128.const i16x8 0x01 0x8081 0x01 0x8081 0x01 0x8081 0x01 0x8081 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const i16x8 0x01 0x81 0x01 0x81 0x01 0x81 0x01 0x81 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const i16x8 0x01 0x8041 0x01 0x8041 0x01 0x8041 0x01 0x8041 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 0xffff 0xfffe 0xfffd 0xfffc 0xfffb 0xfffa 0xfff9 )))
(local.set $expected ( v128.const i16x8 0 0x02 0x04 0x06 0x08 0x0a 0x0c 0x0e ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 2 4 6 8 10 12 14 )))
(local.set $expected ( v128.const i16x8 0 -1 -2 -3 -4 -5 -6 -7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 ) ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 )))
(local.set $expected ( v128.const i16x8 044_444 044_444 044_444 044_444 044_444 044_444 044_444 044_444 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.sub" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 0x0_5678 ) ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 )))
(local.set $expected ( v128.const i16x8 0x0_4444 0x0_4444 0x0_4444 0x0_4444 0x0_4444 0x0_4444 0x0_4444 0x0_4444 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 16383 16383 16383 16383 16383 16383 16383 16383 ) ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 )))
(local.set $expected ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 ) ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16383 -16383 -16383 -16383 -16383 -16383 -16383 -16383 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -16385 -16385 -16385 -16385 -16385 -16385 -16385 -16385 ) ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 )))
(local.set $expected ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32765 32765 32765 32765 32765 32765 32765 32765 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 32765 32765 32765 32765 32765 32765 32765 32765 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32768 32768 32768 32768 32768 32768 32768 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32766 -32766 -32766 -32766 -32766 -32766 -32766 -32766 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ) ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ) ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff 0x3fff ) ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 )))
(local.set $expected ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 ) ( v128.const i16x8 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 0x4000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff -0x3fff ) ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 )))
(local.set $expected ( v128.const i16x8 -16384 -16384 -16384 -16384 -16384 -16384 -16384 -16384 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 ) ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 -0x4000 ) ( v128.const i16x8 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 -0x4001 )))
(local.set $expected ( v128.const i16x8 16384 16384 16384 16384 16384 16384 16384 16384 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i16x8 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ) ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ) ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ) ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x1000 0x1000 0x1000 0x1000 0x1000 0x1000 0x1000 0x1000 ) ( v128.const i8x16 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const i32x4 0x00020002 0x00020002 0x00020002 0x00020002 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff )))
(local.set $expected ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 +0.0 +0.0 +0.0 +0.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 -0.0 -0.0 -0.0 -0.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 1.0 1.0 1.0 1.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ) ( v128.const f32x4 -1.0 -1.0 -1.0 -1.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 +inf +inf +inf +inf )))
(local.set $expected ( v128.const i16x8 0 0x7f80 0 0x7f80 0 0x7f80 0 0x7f80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 -inf -inf -inf -inf )))
(local.set $expected ( v128.const i16x8 0 0xff80 0 0xff80 0 0xff80 0 0xff80 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 ) ( v128.const f32x4 nan nan nan nan )))
(local.set $expected ( v128.const i16x8 0 0x7fc0 0 0x7fc0 0 0x7fc0 0 0x7fc0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 0xffff 0xfffe 0xfffd 0xfffc 0xfffb 0xfffa 0xfff9 )))
(local.set $expected ( v128.const i16x8 0 0xffff 0xfffc 0xfff7 0xfff0 0xffe7 0xffdc 0xffcf ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 2 4 6 8 10 12 14 )))
(local.set $expected ( v128.const i16x8 0 0x02 0x08 0x12 0x20 0x32 0x48 0x62 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 056_789 056_789 056_789 056_789 056_789 056_789 056_789 056_789 )))
(local.set $expected ( v128.const i16x8 021_613 021_613 021_613 021_613 021_613 021_613 021_613 021_613 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.mul" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x0_cdef 0x0_cdef 0x0_cdef 0x0_cdef 0x0_cdef 0x0_cdef 0x0_cdef 0x0_cdef )))
(local.set $expected ( v128.const i16x8 0x0_a28c 0x0_a28c 0x0_a28c 0x0_a28c 0x0_a28c 0x0_a28c 0x0_a28c 0x0_a28c ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 1 1 1 1 1 1 1 1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32766 32766 32766 32766 32766 32766 32766 32766 )))
(local.set $expected ( v128.const i16x8 -32766 -32766 -32766 -32766 -32766 -32766 -32766 -32766 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 -0x01 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 -0x8000 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -0x7fff -0x7fff -0x7fff -0x7fff -0x7fff -0x7fff -0x7fff -0x7fff )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff )))
(local.set $expected ( v128.const i16x8 -32767 -32767 -32767 -32767 -32767 -32767 -32767 -32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.neg" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.neg ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.add ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.sub ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.mul ( i32.const 0 ) ( f32.const 0.0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.neg-arg-empty ( result v128 ) ( i16x8.neg ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.add-1st-arg-empty ( result v128 ) ( i16x8.add ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.add-arg-empty ( result v128 ) ( i16x8.add ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.sub-1st-arg-empty ( result v128 ) ( i16x8.sub ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.sub-arg-empty ( result v128 ) ( i16x8.sub ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.mul-1st-arg-empty ( result v128 ) ( i16x8.mul ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.mul-arg-empty ( result v128 ) ( i16x8.mul ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "add-sub" ) ( param v128 v128 v128 ) ( result v128 ) ( i16x8.add ( i16x8.sub ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "mul-add" ) ( param v128 v128 v128 ) ( result v128 ) ( i16x8.mul ( i16x8.add ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "mul-sub" ) ( param v128 v128 v128 ) ( result v128 ) ( i16x8.mul ( i16x8.sub ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "sub-add" ) ( param v128 v128 v128 ) ( result v128 ) ( i16x8.sub ( i16x8.add ( local.get 0 ) ( local.get 1 ) ) ( local.get 2 ) ) ) ( func ( export "add-neg" ) ( param v128 v128 ) ( result v128 ) ( i16x8.add ( i16x8.neg ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "mul-neg" ) ( param v128 v128 ) ( result v128 ) ( i16x8.mul ( i16x8.neg ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "sub-neg" ) ( param v128 v128 ) ( result v128 ) ( i16x8.sub ( i16x8.neg ( local.get 0 ) ) ( local.get 1 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "add-sub" (func $f (param v128 v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 2 4 6 8 10 12 14 ) ( v128.const i16x8 0 2 4 6 8 10 12 14 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i16x8 0 4 8 12 16 20 24 28 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i16x8 0 2 4 6 8 10 12 14 ) ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 1 2 3 4 5 6 7 )))
(local.set $expected ( v128.const i16x8 0 1 4 9 16 25 36 49 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 2 4 6 8 10 12 14 ) ( v128.const i16x8 0 2 4 6 8 10 12 14 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 1 2 3 4 5 6 7 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 2 2 2 2 2 2 2 2 )))
(local.set $expected ( v128.const i16x8 0 -2 -4 -6 -8 -10 -12 -14 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( v128.const i16x8 0 1 2 3 4 5 6 7 )))
(local.set $expected ( v128.const i16x8 0 -2 -4 -6 -8 -10 -12 -14 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)

