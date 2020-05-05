var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i8x16.splat" ) ( param i32 ) ( result v128 ) ( i8x16.splat ( local.get 0 ) ) ) ( func ( export "i16x8.splat" ) ( param i32 ) ( result v128 ) ( i16x8.splat ( local.get 0 ) ) ) ( func ( export "i32x4.splat" ) ( param i32 ) ( result v128 ) ( i32x4.splat ( local.get 0 ) ) ) ( func ( export "f32x4.splat" ) ( param f32 ) ( result v128 ) ( f32x4.splat ( local.get 0 ) ) ) ( func ( export "i64x2.splat" ) ( param i64 ) ( result v128 ) ( i64x2.splat ( local.get 0 ) ) ) ( func ( export "f64x2.splat" ) ( param f64 ) ( result v128 ) ( f64x2.splat ( local.get 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 5 )))
(local.set $expected ( v128.const i8x16 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -5 )))
(local.set $expected ( v128.const i8x16 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 -5 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 257 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xff )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -128 )))
(local.set $expected ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 127 )))
(local.set $expected ( v128.const i8x16 127 127 127 127 127 127 127 127 127 127 127 127 127 127 127 127 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -129 )))
(local.set $expected ( v128.const i8x16 127 127 127 127 127 127 127 127 127 127 127 127 127 127 127 127 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 128 )))
(local.set $expected ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xff7f )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x80 )))
(local.set $expected ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xAB )))
(local.set $expected ( v128.const i32x4 0xABABABAB 0xABABABAB 0xABABABAB 0xABABABAB ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 5 )))
(local.set $expected ( v128.const i16x8 5 5 5 5 5 5 5 5 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -5 )))
(local.set $expected ( v128.const i16x8 -5 -5 -5 -5 -5 -5 -5 -5 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65537 )))
(local.set $expected ( v128.const i16x8 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xffff )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -32768 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 32767 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -32769 )))
(local.set $expected ( v128.const i16x8 32767 32767 32767 32767 32767 32767 32767 32767 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 32768 )))
(local.set $expected ( v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xffff7fff )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x8000 )))
(local.set $expected ( v128.const i16x8 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 0x8000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xABCD )))
(local.set $expected ( v128.const i32x4 0xABCDABCD 0xABCDABCD 0xABCDABCD 0xABCDABCD ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 012345 )))
(local.set $expected ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x01234 )))
(local.set $expected ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0 0 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 5 )))
(local.set $expected ( v128.const i32x4 5 5 5 5 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -5 )))
(local.set $expected ( v128.const i32x4 -5 -5 -5 -5 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xffffffff )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 4294967295 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -2147483648 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 2147483647 )))
(local.set $expected ( v128.const i32x4 0x7fffffff 0x7fffffff 0x7fffffff 0x7fffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 2147483648 )))
(local.set $expected ( v128.const i32x4 0x80000000 0x80000000 0x80000000 0x80000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 01234567890 )))
(local.set $expected ( v128.const i32x4 012_3456_7890 012_3456_7890 012_3456_7890 012_3456_7890 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x012345678 )))
(local.set $expected ( v128.const i32x4 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0.0 )))
(local.set $expected ( v128.const f32x4 0.0 0.0 0.0 0.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 1.1 )))
(local.set $expected ( v128.const f32x4 1.1 1.1 1.1 1.1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const -1.1 )))
(local.set $expected ( v128.const f32x4 -1.1 -1.1 -1.1 -1.1 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 1e38 )))
(local.set $expected ( v128.const f32x4 1e38 1e38 1e38 1e38 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const -1e38 )))
(local.set $expected ( v128.const f32x4 -1e38 -1e38 -1e38 -1e38 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0x1.fffffep127 )))
(local.set $expected ( v128.const f32x4 0x1.fffffep127 0x1.fffffep127 0x1.fffffep127 0x1.fffffep127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const -0x1.fffffep127 )))
(local.set $expected ( v128.const f32x4 -0x1.fffffep127 -0x1.fffffep127 -0x1.fffffep127 -0x1.fffffep127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0x1p127 )))
(local.set $expected ( v128.const f32x4 0x1p127 0x1p127 0x1p127 0x1p127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const -0x1p127 )))
(local.set $expected ( v128.const f32x4 -0x1p127 -0x1p127 -0x1p127 -0x1p127 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const inf )))
(local.set $expected ( v128.const f32x4 inf inf inf inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const -inf )))
(local.set $expected ( v128.const f32x4 -inf -inf -inf -inf ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const nan )))
(local.set $expected ( v128.const f32x4 nan nan nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0x7FC00000  0x7FC00000  0x7FC00000  0x7FC00000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0123456789 )))
(local.set $expected ( v128.const f32x4 0123456789 0123456789 0123456789 0123456789 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0123456789. )))
(local.set $expected ( v128.const f32x4 0123456789. 0123456789. 0123456789. 0123456789. ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0x0123456789ABCDEF )))
(local.set $expected ( v128.const f32x4 0x0123456789ABCDEF 0x0123456789ABCDEF 0x0123456789ABCDEF 0x0123456789ABCDEF ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0x0123456789ABCDEF. )))
(local.set $expected ( v128.const f32x4 0x0123456789ABCDEF. 0x0123456789ABCDEF. 0x0123456789ABCDEF. 0x0123456789ABCDEF. ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0123456789e019 )))
(local.set $expected ( v128.const f32x4 0123456789e019 0123456789e019 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0123456789.e+019 )))
(local.set $expected ( v128.const f32x4 0123456789.e+019 0123456789.e+019 0123456789.e+019 0123456789.e+019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0x0123456789ABCDEFp019 )))
(local.set $expected ( v128.const f32x4 0x0123456789ABCDEFp019 0x0123456789ABCDEFp019 0x0123456789ABCDEFp019 0x0123456789ABCDEFp019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f32x4.splat" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0x0123456789ABCDEF.p-019 )))
(local.set $expected ( v128.const f32x4 0x0123456789ABCDEF.p-019 0x0123456789ABCDEF.p-019 0x0123456789ABCDEF.p-019 0x0123456789ABCDEF.p-019 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const -0 )))
(local.set $expected ( v128.const i64x2 0 0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 1 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const -1 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 -9223372036854775808 -9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const -9223372036854775808 )))
(local.set $expected ( v128.const i64x2 9223372036854775808 9223372036854775808 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 9223372036854775807 )))
(local.set $expected ( v128.const i64x2 9223372036854775807 9223372036854775807 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 18446744073709551615 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 0x7fffffffffffffff )))
(local.set $expected ( v128.const i64x2 0x7fffffffffffffff 0x7fffffffffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 0xffffffffffffffff )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const -0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 -0x8000000000000000 -0x8000000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const -0x8000000000000000 )))
(local.set $expected ( v128.const i64x2 0x8000000000000000 0x8000000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 01234567890123456789 )))
(local.set $expected ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.splat" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 0x01234567890ABcdef )))
(local.set $expected ( v128.const i64x2 0x0_1234_5678_90AB_cdef 0x0_1234_5678_90AB_cdef ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0.0 )))
(local.set $expected ( v128.const f64x2 0.0 0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -0.0 )))
(local.set $expected ( v128.const f64x2 -0.0 -0.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 1.1 )))
(local.set $expected ( v128.const f64x2 1.1 1.1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -1.1 )))
(local.set $expected ( v128.const f64x2 -1.1 -1.1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x0.0000000000001p-1022 )))
(local.set $expected ( v128.const f64x2 0x0.0000000000001p-1022 0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -0x0.0000000000001p-1022 )))
(local.set $expected ( v128.const f64x2 -0x0.0000000000001p-1022 -0x0.0000000000001p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x1p-1022 )))
(local.set $expected ( v128.const f64x2 0x1p-1022 0x1p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -0x1p-1022 )))
(local.set $expected ( v128.const f64x2 -0x1p-1022 -0x1p-1022 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x1p-1 )))
(local.set $expected ( v128.const f64x2 0x1p-1 0x1p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -0x1p-1 )))
(local.set $expected ( v128.const f64x2 -0x1p-1 -0x1p-1 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x1p+0 )))
(local.set $expected ( v128.const f64x2 0x1p+0 0x1p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1p+0 -0x1p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -0x1.921fb54442d18p+2 )))
(local.set $expected ( v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -0x1.fffffffffffffp+1023 )))
(local.set $expected ( v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const inf )))
(local.set $expected ( v128.const f64x2 inf inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -inf )))
(local.set $expected ( v128.const f64x2 -inf -inf ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -nan )))
(local.set $expected ( v128.const f64x2 -nan -nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const nan )))
(local.set $expected ( v128.const f64x2 nan nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -nan )))
(local.set $expected ( v128.const f64x2 -nan -nan ))

(local.set $result (v128.and (local.get $result) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))
(local.set $expected (v128.and (local.get $expected) (v128.const i32x4  0 0x7FF80000  0 0x7FF80000)))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0123456789 )))
(local.set $expected ( v128.const f64x2 0123456789 0123456789 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0123456789. )))
(local.set $expected ( v128.const f64x2 0123456789. 0123456789. ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x0123456789ABCDEFabcdef )))
(local.set $expected ( v128.const f64x2 0x0123456789ABCDEFabcdef 0x0123456789ABCDEFabcdef ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x0123456789ABCDEFabcdef. )))
(local.set $expected ( v128.const f64x2 0x0123456789ABCDEFabcdef. 0x0123456789ABCDEFabcdef. ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0123456789e019 )))
(local.set $expected ( v128.const f64x2 0123456789e019 0123456789e019 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0123456789e+019 )))
(local.set $expected ( v128.const f64x2 0123456789e+019 0123456789e+019 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x0123456789ABCDEFabcdef.p019 )))
(local.set $expected ( v128.const f64x2 0x0123456789ABCDEFabcdef.p019 0x0123456789ABCDEFabcdef.p019 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "f64x2.splat" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x0123456789ABCDEFabcdef.p-019 )))
(local.set $expected ( v128.const f64x2 0x0123456789ABCDEFabcdef.p-019 0x0123456789ABCDEFabcdef.p-019 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(func (result v128) (v128.splat (i32.const 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i8x16.splat ( i64.const 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i8x16.splat ( f32.const 0.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i8x16.splat ( f64.const 0.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i16x8.splat ( i64.const 1 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i16x8.splat ( f32.const 1.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i16x8.splat ( f64.const 1.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i32x4.splat ( i64.const 2 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i32x4.splat ( f32.const 2.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i32x4.splat ( f64.const 2.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) f32x4.splat ( i32.const 4 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) f32x4.splat ( i64.const 4 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) f32x4.splat ( f64.const 4.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i64x2.splat ( i32.const 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) i64x2.splat ( f64.const 0.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) f64x2.splat ( i32.const 0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) f64x2.splat ( f32.const 0.0 ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "as-v128_store-operand-1" ) ( param i32 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( i8x16.splat ( local.get 0 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-2" ) ( param i32 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( i16x8.splat ( local.get 0 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-3" ) ( param i32 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( i32x4.splat ( local.get 0 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-4" ) ( param i64 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( i64x2.splat ( local.get 0 ) ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "as-v128_store-operand-5" ) ( param f64 ) ( result v128 ) ( v128.store ( i32.const 0 ) ( f64x2.splat ( local.get 0 ) ) ) ( v128.load ( i32.const 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 256 )))
(local.set $expected ( v128.const i16x8 0x100 0x100 0x100 0x100 0x100 0x100 0x100 0x100 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-3" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xffffffff )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-4" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 1 )))
(local.set $expected ( v128.const i64x2 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_store-operand-5" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const -0x1p+0 )))
(local.set $expected ( v128.const f64x2 -0x1p+0 -0x1p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "as-i8x16_extract_lane_s-operand-first" ) ( param i32 ) ( result i32 ) ( i8x16.extract_lane_s 0 ( i8x16.splat ( local.get 0 ) ) ) ) ( func ( export "as-i8x16_extract_lane_s-operand-last" ) ( param i32 ) ( result i32 ) ( i8x16.extract_lane_s 15 ( i8x16.splat ( local.get 0 ) ) ) ) ( func ( export "as-i16x8_extract_lane_s-operand-first" ) ( param i32 ) ( result i32 ) ( i16x8.extract_lane_s 0 ( i16x8.splat ( local.get 0 ) ) ) ) ( func ( export "as-i16x8_extract_lane_s-operand-last" ) ( param i32 ) ( result i32 ) ( i16x8.extract_lane_s 7 ( i16x8.splat ( local.get 0 ) ) ) ) ( func ( export "as-i32x4_extract_lane_s-operand-first" ) ( param i32 ) ( result i32 ) ( i32x4.extract_lane 0 ( i32x4.splat ( local.get 0 ) ) ) ) ( func ( export "as-i32x4_extract_lane_s-operand-last" ) ( param i32 ) ( result i32 ) ( i32x4.extract_lane 3 ( i32x4.splat ( local.get 0 ) ) ) ) ( func ( export "as-f32x4_extract_lane_s-operand-first" ) ( param f32 ) ( result f32 ) ( f32x4.extract_lane 0 ( f32x4.splat ( local.get 0 ) ) ) ) ( func ( export "as-f32x4_extract_lane_s-operand-last" ) ( param f32 ) ( result f32 ) ( f32x4.extract_lane 3 ( f32x4.splat ( local.get 0 ) ) ) ) ( func ( export "as-v8x16_swizzle-operands" ) ( param i32 ) ( param i32 ) ( result v128 ) ( v8x16.swizzle ( i8x16.splat ( local.get 0 ) ) ( i8x16.splat ( local.get 1 ) ) ) ) ( func ( export "as-i64x2_extract_lane-operand-first" ) ( param i64 ) ( result i64 ) ( i64x2.extract_lane 0 ( i64x2.splat ( local.get 0 ) ) ) ) ( func ( export "as-i64x2_extract_lane-operand-last" ) ( param i64 ) ( result i64 ) ( i64x2.extract_lane 1 ( i64x2.splat ( local.get 0 ) ) ) ) ( func ( export "as-f64x2_extract_lane-operand-first" ) ( param f64 ) ( result f64 ) ( f64x2.extract_lane 0 ( f64x2.splat ( local.get 0 ) ) ) ) ( func ( export "as-f64x2_extract_lane-operand-last" ) ( param f64 ) ( result f64 ) ( f64x2.extract_lane 1 ( f64x2.splat ( local.get 0 ) ) ) ) ( func ( export "as-i8x16_add_sub-operands" ) ( param i32 i32 i32 ) ( result v128 ) ( i8x16.add ( i8x16.splat ( local.get 0 ) ) ( i8x16.sub ( i8x16.splat ( local.get 1 ) ) ( i8x16.splat ( local.get 2 ) ) ) ) ) ( func ( export "as-i16x8_add_sub_mul-operands" ) ( param i32 i32 i32 i32 ) ( result v128 ) ( i16x8.add ( i16x8.splat ( local.get 0 ) ) ( i16x8.sub ( i16x8.splat ( local.get 1 ) ) ( i16x8.mul ( i16x8.splat ( local.get 2 ) ) ( i16x8.splat ( local.get 3 ) ) ) ) ) ) ( func ( export "as-i32x4_add_sub_mul-operands" ) ( param i32 i32 i32 i32 ) ( result v128 ) ( i32x4.add ( i32x4.splat ( local.get 0 ) ) ( i32x4.sub ( i32x4.splat ( local.get 1 ) ) ( i32x4.mul ( i32x4.splat ( local.get 2 ) ) ( i32x4.splat ( local.get 3 ) ) ) ) ) ) ( func ( export "as-i64x2_add_sub_mul-operands" ) ( param i64 i64 i64 i64 ) ( result v128 ) ( i64x2.add ( i64x2.splat ( local.get 0 ) ) ( i64x2.sub ( i64x2.splat ( local.get 1 ) ) ( i64x2.mul ( i64x2.splat ( local.get 2 ) ) ( i64x2.splat ( local.get 3 ) ) ) ) ) ) ( func ( export "as-f64x2_add_sub_mul-operands" ) ( param f64 f64 f64 f64 ) ( result v128 ) ( f64x2.add ( f64x2.splat ( local.get 0 ) ) ( f64x2.sub ( f64x2.splat ( local.get 1 ) ) ( f64x2.mul ( f64x2.splat ( local.get 2 ) ) ( f64x2.splat ( local.get 3 ) ) ) ) ) ) ( func ( export "as-i8x16_add_saturate_s-operands" ) ( param i32 i32 ) ( result v128 ) ( i8x16.add_saturate_s ( i8x16.splat ( local.get 0 ) ) ( i8x16.splat ( local.get 1 ) ) ) ) ( func ( export "as-i16x8_add_saturate_s-operands" ) ( param i32 i32 ) ( result v128 ) ( i16x8.add_saturate_s ( i16x8.splat ( local.get 0 ) ) ( i16x8.splat ( local.get 1 ) ) ) ) ( func ( export "as-i8x16_sub_saturate_u-operands" ) ( param i32 i32 ) ( result v128 ) ( i8x16.sub_saturate_u ( i8x16.splat ( local.get 0 ) ) ( i8x16.splat ( local.get 1 ) ) ) ) ( func ( export "as-i16x8_sub_saturate_u-operands" ) ( param i32 i32 ) ( result v128 ) ( i16x8.sub_saturate_u ( i16x8.splat ( local.get 0 ) ) ( i16x8.splat ( local.get 1 ) ) ) ) ( func ( export "as-i8x16_shr_s-operand" ) ( param i32 i32 ) ( result v128 ) ( i8x16.shr_s ( i8x16.splat ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "as-i16x8_shr_s-operand" ) ( param i32 i32 ) ( result v128 ) ( i16x8.shr_s ( i16x8.splat ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "as-i32x4_shr_s-operand" ) ( param i32 i32 ) ( result v128 ) ( i32x4.shr_s ( i32x4.splat ( local.get 0 ) ) ( local.get 1 ) ) ) ( func ( export "as-v128_and-operands" ) ( param i32 i32 ) ( result v128 ) ( v128.and ( i8x16.splat ( local.get 0 ) ) ( i8x16.splat ( local.get 1 ) ) ) ) ( func ( export "as-v128_or-operands" ) ( param i32 i32 ) ( result v128 ) ( v128.or ( i16x8.splat ( local.get 0 ) ) ( i16x8.splat ( local.get 1 ) ) ) ) ( func ( export "as-v128_xor-operands" ) ( param i32 i32 ) ( result v128 ) ( v128.xor ( i32x4.splat ( local.get 0 ) ) ( i32x4.splat ( local.get 1 ) ) ) ) ( func ( export "as-i8x16_all_true-operand" ) ( param i32 ) ( result i32 ) ( i8x16.all_true ( i8x16.splat ( local.get 0 ) ) ) ) ( func ( export "as-i16x8_all_true-operand" ) ( param i32 ) ( result i32 ) ( i16x8.all_true ( i16x8.splat ( local.get 0 ) ) ) ) ( func ( export "as-i32x4_all_true-operand1" ) ( param i32 ) ( result i32 ) ( i32x4.all_true ( i32x4.splat ( local.get 0 ) ) ) ) ( func ( export "as-i32x4_all_true-operand2" ) ( param i64 ) ( result i32 ) ( i32x4.all_true ( i64x2.splat ( local.get 0 ) ) ) ) ( func ( export "as-i8x16_eq-operands" ) ( param i32 i32 ) ( result v128 ) ( i8x16.eq ( i8x16.splat ( local.get 0 ) ) ( i8x16.splat ( local.get 1 ) ) ) ) ( func ( export "as-i16x8_eq-operands" ) ( param i32 i32 ) ( result v128 ) ( i16x8.eq ( i16x8.splat ( local.get 0 ) ) ( i16x8.splat ( local.get 1 ) ) ) ) ( func ( export "as-i32x4_eq-operands1" ) ( param i32 i32 ) ( result v128 ) ( i32x4.eq ( i32x4.splat ( local.get 0 ) ) ( i32x4.splat ( local.get 1 ) ) ) ) ( func ( export "as-i32x4_eq-operands2" ) ( param i64 i64 ) ( result v128 ) ( i32x4.eq ( i64x2.splat ( local.get 0 ) ) ( i64x2.splat ( local.get 1 ) ) ) ) ( func ( export "as-f32x4_eq-operands" ) ( param f32 f32 ) ( result v128 ) ( f32x4.eq ( f32x4.splat ( local.get 0 ) ) ( f32x4.splat ( local.get 1 ) ) ) ) ( func ( export "as-f64x2_eq-operands" ) ( param f64 f64 ) ( result v128 ) ( f64x2.eq ( f64x2.splat ( local.get 0 ) ) ( f64x2.splat ( local.get 1 ) ) ) ) ( func ( export "as-f32x4_abs-operand" ) ( param f32 ) ( result v128 ) ( f32x4.abs ( f32x4.splat ( local.get 0 ) ) ) ) ( func ( export "as-f32x4_min-operands" ) ( param f32 f32 ) ( result v128 ) ( f32x4.min ( f32x4.splat ( local.get 0 ) ) ( f32x4.splat ( local.get 1 ) ) ) ) ( func ( export "as-f32x4_div-operands" ) ( param f32 f32 ) ( result v128 ) ( f32x4.div ( f32x4.splat ( local.get 0 ) ) ( f32x4.splat ( local.get 1 ) ) ) ) ( func ( export "as-f32x4_convert_s_i32x4-operand" ) ( param i32 ) ( result v128 ) ( f32x4.convert_i32x4_s ( i32x4.splat ( local.get 0 ) ) ) ) ( func ( export "as-i32x4_trunc_s_f32x4_sat-operand" ) ( param f32 ) ( result v128 ) ( i32x4.trunc_sat_f32x4_s ( f32x4.splat ( local.get 0 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_extract_lane_s-operand-first" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const 42 )))
(local.set $expected ( i32.const 42 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_extract_lane_s-operand-last" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const -42 )))
(local.set $expected ( i32.const -42 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_extract_lane_s-operand-first" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const 0xffff7fff )))
(local.set $expected ( i32.const 32767 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_extract_lane_s-operand-last" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const 0x8000 )))
(local.set $expected ( i32.const -32768 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_extract_lane_s-operand-first" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const 0x7fffffff )))
(local.set $expected ( i32.const 2147483647 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_extract_lane_s-operand-last" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const 0x80000000 )))
(local.set $expected ( i32.const -2147483648 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f32x4_extract_lane_s-operand-first" (func $f (param f32) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( f32.const 1.5 )))
(local.set $expected ( f32.const 1.5 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f32x4_extract_lane_s-operand-last" (func $f (param f32) (result f32)))
  (func (export "run") (result i32) 
(local $result f32)
(local $expected f32)
(local $cmpresult i32)

(local.set $result (call $f ( f32.const -0.25 )))
(local.set $expected ( f32.const -0.25 ))

(local.set $cmpresult (f32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v8x16_swizzle-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 ) ( i32.const -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i64x2_extract_lane-operand-last" (func $f (param i64) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( i64.const -42 )))
(local.set $expected ( i64.const -42 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i64x2_extract_lane-operand-first" (func $f (param i64) (result i64)))
  (func (export "run") (result i32) 
(local $result i64)
(local $expected i64)
(local $cmpresult i32)

(local.set $result (call $f ( i64.const 42 )))
(local.set $expected ( i64.const 42 ))

(local.set $cmpresult (i64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f64x2_extract_lane-operand-first" (func $f (param f64) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( f64.const 1.5 )))
(local.set $expected ( f64.const 1.5 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f64x2_extract_lane-operand-last" (func $f (param f64) (result f64)))
  (func (export "run") (result i32) 
(local $result f64)
(local $expected f64)
(local $cmpresult i32)

(local.set $result (call $f ( f64.const -0x1p+0 )))
(local.set $expected ( f64.const -0x1p+0 ))

(local.set $cmpresult (f64.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_add_sub-operands" (func $f (param i32 i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 3 ) ( i32.const 2 ) ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 4 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_add_sub_mul-operands" (func $f (param i32 i32 i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 257 ) ( i32.const 128 ) ( i32.const 16 ) ( i32.const 16 )))
(local.set $expected ( v128.const i16x8 129 129 129 129 129 129 129 129 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_add_sub_mul-operands" (func $f (param i32 i32 i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65535 ) ( i32.const 65537 ) ( i32.const 256 ) ( i32.const 256 )))
(local.set $expected ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i64x2_add_sub_mul-operands" (func $f (param i64 i64 i64 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 0x7fffffff ) ( i64.const 0x1_0000_0001 ) ( i64.const 65536 ) ( i64.const 65536 )))
(local.set $expected ( v128.const i64x2 0x8000_0000 0x8000_0000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f64x2_add_sub_mul-operands" (func $f (param f64 f64 f64 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 0x1p-1 ) ( f64.const 0.75 ) ( f64.const 0x1p-1 ) ( f64.const 0.5 )))
(local.set $expected ( v128.const f64x2 0x1p+0 0x1p+0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_add_saturate_s-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x7f ) ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f 0x7f ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_add_saturate_s-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x7fff ) ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff 0x7fff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_sub_saturate_u-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x7f ) ( i32.const 0xff )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_sub_saturate_u-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x7fff ) ( i32.const 0xffff )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_shr_s-operand" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xf0 ) ( i32.const 3 )))
(local.set $expected ( v128.const i8x16 -2 -2 -2 -2 -2 -2 -2 -2 -2 -2 -2 -2 -2 -2 -2 -2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_shr_s-operand" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x100 ) ( i32.const 4 )))
(local.set $expected ( v128.const i16x8 16 16 16 16 16 16 16 16 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_shr_s-operand" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -1 ) ( i32.const 16 )))
(local.set $expected ( v128.const i32x4 -1 -1 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_and-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x11 ) ( i32.const 0xff )))
(local.set $expected ( v128.const i8x16 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 17 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_or-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 ) ( i32.const 0xffff )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-v128_xor-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xf0f0f0f0 ) ( i32.const 0xffffffff )))
(local.set $expected ( v128.const i32x4 0xf0f0f0f 0xf0f0f0f 0xf0f0f0f 0xf0f0f0f ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_all_true-operand" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_all_true-operand" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const 0xffff )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_all_true-operand1" (func $f (param i32) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i32.const 0xf0f0f0f0 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_all_true-operand2" (func $f (param i64) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ( i64.const -1 )))
(local.set $expected ( i32.const 1 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i8x16_eq-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 ) ( i32.const 2 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i16x8_eq-operands" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -1 ) ( i32.const 65535 )))
(local.set $expected ( v128.const i16x8 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff 0xffff ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_eq-operands1" (func $f (param i32 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const -1 ) ( i32.const 0xffffffff )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f32x4_eq-operands" (func $f (param f32 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const +0.0 ) ( f32.const -0.0 )))
(local.set $expected ( v128.const i32x4 0xffffffff 0xffffffff 0xffffffff 0xffffffff ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_eq-operands2" (func $f (param i64 i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 1 ) ( i64.const 2 )))
(local.set $expected ( v128.const i64x2 0xffffffff00000000 0xffffffff00000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f64x2_eq-operands" (func $f (param f64 f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const +0.0 ) ( f64.const -0.0 )))
(local.set $expected ( v128.const i64x2 -1 -1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f32x4_abs-operand" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const -1.125 )))
(local.set $expected ( v128.const f32x4 1.125 1.125 1.125 1.125 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f32x4_min-operands" (func $f (param f32 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 0.25 ) ( f32.const 1e-38 )))
(local.set $expected ( v128.const f32x4 1e-38 1e-38 1e-38 1e-38 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f32x4_div-operands" (func $f (param f32 f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 1.0 ) ( f32.const 8.0 )))
(local.set $expected ( v128.const f32x4 0.125 0.125 0.125 0.125 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-f32x4_convert_s_i32x4-operand" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 12345 )))
(local.set $expected ( v128.const f32x4 12345.0 12345.0 12345.0 12345.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-i32x4_trunc_s_f32x4_sat-operand" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 1.1 )))
(local.set $expected ( v128.const i32x4 1 1 1 1 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( global $g ( mut v128 ) ( v128.const f32x4 0.0 0.0 0.0 0.0 ) ) ( func ( export "as-br-value1" ) ( param i32 ) ( result v128 ) ( block ( result v128 ) ( br 0 ( i8x16.splat ( local.get 0 ) ) ) ) ) ( func ( export "as-return-value1" ) ( param i32 ) ( result v128 ) ( return ( i16x8.splat ( local.get 0 ) ) ) ) ( func ( export "as-local_set-value1" ) ( param i32 ) ( result v128 ) ( local v128 ) ( local.set 1 ( i32x4.splat ( local.get 0 ) ) ) ( return ( local.get 1 ) ) ) ( func ( export "as-global_set-value1" ) ( param f32 ) ( result v128 ) ( global.set $g ( f32x4.splat ( local.get 0 ) ) ) ( return ( global.get $g ) ) ) ( func ( export "as-br-value2" ) ( param i64 ) ( result v128 ) ( block ( result v128 ) ( br 0 ( i64x2.splat ( local.get 0 ) ) ) ) ) ( func ( export "as-return-value2" ) ( param i64 ) ( result v128 ) ( return ( i64x2.splat ( local.get 0 ) ) ) ) ( func ( export "as-local_set-value2" ) ( param i64 ) ( result v128 ) ( local v128 ) ( local.set 1 ( i64x2.splat ( local.get 0 ) ) ) ( return ( local.get 1 ) ) ) ( func ( export "as-global_set-value2" ) ( param f64 ) ( result v128 ) ( global.set $g ( f64x2.splat ( local.get 0 ) ) ) ( return ( global.get $g ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-br-value1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xAB )))
(local.set $expected ( v128.const i8x16 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB 0xAB ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-return-value1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0xABCD )))
(local.set $expected ( v128.const i16x8 0xABCD 0xABCD 0xABCD 0xABCD 0xABCD 0xABCD 0xABCD 0xABCD ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-local_set-value1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0x10000 )))
(local.set $expected ( v128.const i32x4 0x10000 0x10000 0x10000 0x10000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-global_set-value1" (func $f (param f32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f32.const 1.0 )))
(local.set $expected ( v128.const f32x4 1.0 1.0 1.0 1.0 ))

(local.set $cmpresult (f32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-br-value2" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 0xABCD )))
(local.set $expected ( v128.const i64x2 0xABCD 0xABCD ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-return-value2" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 0xABCD )))
(local.set $expected ( v128.const i64x2 0xABCD 0xABCD ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-local_set-value2" (func $f (param i64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i64.const 0x10000 )))
(local.set $expected ( v128.const i64x2 0x10000 0x10000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-global_set-value2" (func $f (param f64) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( f64.const 1.0 )))
(local.set $expected ( v128.const f64x2 1.0 1.0 ))

(local.set $cmpresult (f64x2.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.splat-arg-empty ( result v128 ) ( i8x16.splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.splat-arg-empty ( result v128 ) ( i16x8.splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.splat-arg-empty ( result v128 ) ( i32x4.splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f32x4.splat-arg-empty ( result v128 ) ( f32x4.splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.splat-arg-empty ( result v128 ) ( i64x2.splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $f64x2.splat-arg-empty ( result v128 ) ( f64x2.splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)

