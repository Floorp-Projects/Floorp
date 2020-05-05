var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "i8x16.shl" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i8x16.shl ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i8x16.shr_s" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i8x16.shr_s ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i8x16.shr_u" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i8x16.shr_u ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i16x8.shl" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i16x8.shl ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i16x8.shr_s" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i16x8.shr_s ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i16x8.shr_u" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i16x8.shr_u ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i32x4.shl" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i32x4.shl ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i32x4.shr_s" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i32x4.shr_s ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i32x4.shr_u" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i32x4.shr_u ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i64x2.shl" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i64x2.shl ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i64x2.shr_s" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i64x2.shr_s ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i64x2.shr_u" ) ( param $0 v128 ) ( param $1 i32 ) ( result v128 ) ( i64x2.shr_u ( local.get $0 ) ( local.get $1 ) ) ) ( func ( export "i8x16.shl_1" ) ( param $0 v128 ) ( result v128 ) ( i8x16.shl ( local.get $0 ) ( i32.const 1 ) ) ) ( func ( export "i8x16.shr_u_8" ) ( param $0 v128 ) ( result v128 ) ( i8x16.shr_u ( local.get $0 ) ( i32.const 8 ) ) ) ( func ( export "i8x16.shr_s_9" ) ( param $0 v128 ) ( result v128 ) ( i8x16.shr_s ( local.get $0 ) ( i32.const 9 ) ) ) ( func ( export "i16x8.shl_1" ) ( param $0 v128 ) ( result v128 ) ( i16x8.shl ( local.get $0 ) ( i32.const 1 ) ) ) ( func ( export "i16x8.shr_u_16" ) ( param $0 v128 ) ( result v128 ) ( i16x8.shr_u ( local.get $0 ) ( i32.const 16 ) ) ) ( func ( export "i16x8.shr_s_17" ) ( param $0 v128 ) ( result v128 ) ( i16x8.shr_s ( local.get $0 ) ( i32.const 17 ) ) ) ( func ( export "i32x4.shl_1" ) ( param $0 v128 ) ( result v128 ) ( i32x4.shl ( local.get $0 ) ( i32.const 1 ) ) ) ( func ( export "i32x4.shr_u_32" ) ( param $0 v128 ) ( result v128 ) ( i32x4.shr_u ( local.get $0 ) ( i32.const 32 ) ) ) ( func ( export "i32x4.shr_s_33" ) ( param $0 v128 ) ( result v128 ) ( i32x4.shr_s ( local.get $0 ) ( i32.const 33 ) ) ) ( func ( export "i64x2.shl_1" ) ( param $0 v128 ) ( result v128 ) ( i64x2.shl ( local.get $0 ) ( i32.const 1 ) ) ) ( func ( export "i64x2.shr_u_64" ) ( param $0 v128 ) ( result v128 ) ( i64x2.shr_u ( local.get $0 ) ( i32.const 64 ) ) ) ( func ( export "i64x2.shr_s_65" ) ( param $0 v128 ) ( result v128 ) ( i64x2.shr_s ( local.get $0 ) ( i32.const 65 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -64 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D ) ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 0 -128 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF 0xA0 0xB0 0xC0 0xD0 0xE0 0xF0 0x0A 0x0B 0x0C 0x0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i8x16 0xA0 0xB0 0xC0 0xD0 0xE0 0xF0 0x00 0x00 0x00 0x00 0x00 0x00 0xA0 0xB0 0xC0 0xD0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 8 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -64 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D ) ( i32.const 9 )))
(local.set $expected ( v128.const i8x16 0 -128 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 9 )))
(local.set $expected ( v128.const i8x16 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A 0x1C 0x1E ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 17 )))
(local.set $expected ( v128.const i8x16 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A 0x1C 0x1E ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 33 )))
(local.set $expected ( v128.const i8x16 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A 0x1C 0x1E ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i8x16 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A 0x1C 0x1E ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i8x16 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A 0x1C 0x1E ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i8x16 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A 0x1C 0x1E ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i8x16 0 4 8 12 16 20 24 28 32 36 0x28 0x2C 0x30 0x34 0x38 0x3C ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -64 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D ) ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 64 96 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF 0xA0 0xB0 0xC0 0xD0 0xE0 0xF0 0x0A 0x0B 0x0C 0x0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i8x16 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F 0x00 0x00 0x00 0x00 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 8 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -64 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D ) ( i32.const 9 )))
(local.set $expected ( v128.const i8x16 64 96 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 9 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 17 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 33 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 0x02 0x02 0x03 0x03 0x03 0x03 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -64 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D ) ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 192 224 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xAA 0xBB 0xCC 0xDD 0xEE 0xFF 0xA0 0xB0 0xC0 0xD0 0xE0 0xF0 0x0A 0x0B 0x0C 0x0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i8x16 0xFA 0xFB 0xFC 0xFD 0xFE 0xFF 0xFA 0xFB 0xFC 0xFD 0xFE 0xFF 0x00 0x00 0x00 0x00 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 8 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 -128 -64 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D ) ( i32.const 9 )))
(local.set $expected ( v128.const i8x16 192 224 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 9 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 17 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 33 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 1 1 1 1 2 2 0x02 0x02 0x03 0x03 0x03 0x03 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl_1" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F )))
(local.set $expected ( v128.const i8x16 0 2 4 6 8 10 12 14 16 18 0x14 0x16 0x18 0x1A 0x1C 0x1E ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u_8" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F )))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s_9" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 0x0A 0x0B 0x0C 0x0D 0x0e 0x0F )))
(local.set $expected ( v128.const i8x16 0 0 1 1 2 2 3 3 4 4 0x05 0x05 0x06 0x06 0x07 0x07 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -64 0 1 2 3 4 5 ) ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 65280 65408 0 2 4 6 8 10 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( i32.const 2 )))
(local.set $expected ( v128.const i16x8 49380 49380 49380 49380 49380 49380 49380 49380 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( i32.const 2 )))
(local.set $expected ( v128.const i16x8 0x48d0 0x48d0 0x48d0 0x48d0 0x48d0 0x48d0 0x48d0 0x48d0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xAABB 0xCCDD 0xEEFF 0xA0B0 0xC0D0 0xE0F0 0x0A0B 0x0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i16x8 0xABB0 0xCDD0 0xEFF0 0xB00 0xD00 0xF00 0xA0B0 0xC0D0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 8 )))
(local.set $expected ( v128.const i16x8 0 256 512 768 1024 1280 1536 1792 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 32 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 128 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 256 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -64 0 1 2 3 4 5 ) ( i32.const 17 )))
(local.set $expected ( v128.const i16x8 65280 65408 0 2 4 6 8 10 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 17 )))
(local.set $expected ( v128.const i16x8 0 2 4 6 8 10 12 14 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 33 )))
(local.set $expected ( v128.const i16x8 0 2 4 6 8 10 12 14 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 129 )))
(local.set $expected ( v128.const i16x8 0 2 4 6 8 10 12 14 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 257 )))
(local.set $expected ( v128.const i16x8 0 2 4 6 8 10 12 14 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 513 )))
(local.set $expected ( v128.const i16x8 0 2 4 6 8 10 12 14 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 514 )))
(local.set $expected ( v128.const i16x8 0 4 8 12 16 20 24 28 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -64 0 1 2 3 4 5 ) ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 32704 32736 0 0 1 1 2 2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( i32.const 2 )))
(local.set $expected ( v128.const i16x8 3086 3086 3086 3086 3086 3086 3086 3086 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB ) ( i32.const 2 )))
(local.set $expected ( v128.const i16x8 0x242a 0x242a 0x242a 0x242a 0x242a 0x242a 0x242a 0x242a ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xAABB 0xCCDD 0xEEFF 0xA0B0 0xC0D0 0xE0F0 0x0A0B 0x0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i16x8 0xAAB 0xCCD 0xEEF 0xA0B 0xC0D 0xE0F 0x0A0 0x0C0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 8 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 32 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 128 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 256 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -64 0 1 2 3 4 5 ) ( i32.const 17 )))
(local.set $expected ( v128.const i16x8 32704 32736 0 0 1 1 2 2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 17 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 33 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 129 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 257 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 513 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 514 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -64 0 1 2 3 4 5 ) ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 65472 65504 0 0 1 1 2 2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( i32.const 2 )))
(local.set $expected ( v128.const i16x8 3086 3086 3086 3086 3086 3086 3086 3086 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB 0x0_90AB ) ( i32.const 2 )))
(local.set $expected ( v128.const i16x8 0xe42a 0xe42a 0xe42a 0xe42a 0xe42a 0xe42a 0xe42a 0xe42a ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xAABB 0xCCDD 0xEEFF 0xA0B0 0xC0D0 0xE0F0 0x0A0B 0x0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i16x8 0xFAAB 0xFCCD 0xFEEF 0xFA0B 0xFC0D 0xFE0F 0x00A0 0x00C0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 8 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 32 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 128 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 256 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -64 0 1 2 3 4 5 ) ( i32.const 17 )))
(local.set $expected ( v128.const i16x8 65472 65504 0 0 1 1 2 2 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 17 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 33 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 129 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 257 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 513 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ( i32.const 514 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 1 1 1 1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl_1" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 )))
(local.set $expected ( v128.const i16x8 0 2 4 6 8 10 12 14 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u_16" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 )))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s_17" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 1 2 3 4 5 6 7 )))
(local.set $expected ( v128.const i16x8 0 0 1 1 2 2 3 3 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -32768 0 0x0A0B0C0D ) ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 0 4294901760 0 0x1416181A ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 01_234_567_890 01_234_567_890 01_234_567_890 01_234_567_890 ) ( i32.const 2 )))
(local.set $expected ( v128.const i32x4 643304264 643304264 643304264 643304264 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 ) ( i32.const 2 )))
(local.set $expected ( v128.const i32x4 0x48d159e0 0x48d159e0 0x48d159e0 0x48d159e0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xAABBCCDD 0xEEFFA0B0 0xC0D0E0F0 0x0A0B0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i32x4 0xABBCCDD0 0xEFFA0B00 0x0D0E0F00 0xA0B0C0D0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 8 )))
(local.set $expected ( v128.const i32x4 0 256 0x00000E00 0x00000F00 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -32768 0 0x0A0B0C0D ) ( i32.const 33 )))
(local.set $expected ( v128.const i32x4 0 4294901760 0 0x1416181A ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 33 )))
(local.set $expected ( v128.const i32x4 0 2 0x1C 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 65 )))
(local.set $expected ( v128.const i32x4 0 2 0x1C 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i32x4 0 2 0x1C 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i32x4 0 2 0x1C 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i32x4 0 2 0x1C 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i32x4 0 4 0x38 0x3C ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -32768 0x0000000C 0x0000000D ) ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 1073741824 2147467264 0x00000006 0x00000006 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 01_234_567_890 01_234_567_890 01_234_567_890 01_234_567_890 ) ( i32.const 2 )))
(local.set $expected ( v128.const i32x4 308641972 308641972 308641972 308641972 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef ) ( i32.const 2 )))
(local.set $expected ( v128.const i32x4 0x242af37b 0x242af37b 0x242af37b 0x242af37b ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xAABBCCDD 0xEEFFA0B0 0xC0D0E0F0 0x0A0B0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i32x4 0x0AABBCCD 0x0EEFFA0B 0x0C0D0E0F 0x00A0B0C0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 8 )))
(local.set $expected ( v128.const i32x4 0 0 0x00000000 0x00000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -32768 0x0000000C 0x0000000D ) ( i32.const 33 )))
(local.set $expected ( v128.const i32x4 1073741824 2147467264 0x00000006 0x00000006 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 33 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 65 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i32x4 0 0 0x03 0x03 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -32768 0x0C 0x0D ) ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 3221225472 4294950912 0x06 0x06 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 01_234_567_890 01_234_567_890 01_234_567_890 01_234_567_890 ) ( i32.const 2 )))
(local.set $expected ( v128.const i32x4 308641972 308641972 308641972 308641972 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef 0x0_90AB_cdef ) ( i32.const 2 )))
(local.set $expected ( v128.const i32x4 0xe42af37b 0xe42af37b 0xe42af37b 0xe42af37b ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0xAABBCCDD 0xEEFFA0B0 0xC0D0E0F0 0x0A0B0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i32x4 0xfaabbccd 0xFEEFFA0B 0xFC0D0E0F 0x00A0B0C0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 8 )))
(local.set $expected ( v128.const i32x4 0 0 0x00000000 0x00000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 -2147483648 -32768 0x0C 0x0D ) ( i32.const 33 )))
(local.set $expected ( v128.const i32x4 3221225472 4294950912 0x06 0x06 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 33 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 65 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i32x4 0 0 0x07 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i32x4 0 0 0x03 0x03 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl_1" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F )))
(local.set $expected ( v128.const i32x4 0 2 28 30 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u_32" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F )))
(local.set $expected ( v128.const i32x4 0 1 0x0E 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s_33" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i32x4 0 1 0x0E 0x0F )))
(local.set $expected ( v128.const i32x4 0 0 7 7 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -2147483648 ) ( i32.const 1 )))
(local.set $expected ( v128.const i64x2 0 18446744069414584320 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 ) ( i32.const 2 )))
(local.set $expected ( v128.const i64x2 4938271560493827156 4938271560493827156 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x0_1234_5678_90AB_cdef 0x0_1234_5678_90AB_cdef ) ( i32.const 2 )))
(local.set $expected ( v128.const i64x2 0x48d159e242af37bc 0x48d159e242af37bc ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xAABBCCDDEEFFA0B0 0xC0D0E0F00A0B0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i64x2 0xABBCCDDEEFFA0B00 0xD0E0F00A0B0C0D0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xAABBCCDDEEFFA0B0 0xC0D0E0F00A0B0C0D ) ( i32.const 8 )))
(local.set $expected ( v128.const i64x2 0xBBCCDDEEFFA0B000 0xD0E0F00A0B0C0D00 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 16 )))
(local.set $expected ( v128.const i64x2 65536 0xF0000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i64x2 4294967296 0xF00000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i64x2 1 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i64x2 1 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 65 )))
(local.set $expected ( v128.const i64x2 2 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i64x2 2 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i64x2 2 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i64x2 2 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i64x2 4 0x3C ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -2147483648 ) ( i32.const 1 )))
(local.set $expected ( v128.const i64x2 4611686018427387904 9223372035781033984 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 ) ( i32.const 2 )))
(local.set $expected ( v128.const i64x2 308641972530864197 308641972530864197 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x0_90AB_cdef_8765_4321 0x0_90AB_cdef_8765_4321 ) ( i32.const 2 )))
(local.set $expected ( v128.const i64x2 0x242af37be1d950c8 0x242af37be1d950c8 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xAABBCCDDEEFFA0B0 0xC0D0E0F00A0B0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i64x2 0xAABBCCDDEEFFA0B 0xC0D0E0F00A0B0C0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xAABBCCDDEEFFA0B0 0xC0D0E0F00A0B0C0D ) ( i32.const 8 )))
(local.set $expected ( v128.const i64x2 0xAABBCCDDEEFFA0 0xC0D0E0F00A0B0C ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 16 )))
(local.set $expected ( v128.const i64x2 0 0x00 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i64x2 0 0x00 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i64x2 1 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i64x2 1 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 65 )))
(local.set $expected ( v128.const i64x2 0 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i64x2 0 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i64x2 0 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i64x2 0 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i64x2 0 0x03 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -2147483648 ) ( i32.const 1 )))
(local.set $expected ( v128.const i64x2 13835058055282163712 18446744072635809792 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 01_234_567_890_123_456_789 01_234_567_890_123_456_789 ) ( i32.const 2 )))
(local.set $expected ( v128.const i64x2 308641972530864197 308641972530864197 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x0_90AB_cdef_8765_4321 0x0_90AB_cdef_8765_4321 ) ( i32.const 2 )))
(local.set $expected ( v128.const i64x2 0xe42af37be1d950c8 0xe42af37be1d950c8 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xAABBCCDDEEFFA0B0 0xC0D0E0F00A0B0C0D ) ( i32.const 4 )))
(local.set $expected ( v128.const i64x2 0xFAABBCCDDEEFFA0B 0xFC0D0E0F00A0B0C0 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0xFFAABBCCDDEEFFA0 0xC0D0E0F00A0B0C0D ) ( i32.const 8 )))
(local.set $expected ( v128.const i64x2 0xFFFFAABBCCDDEEFF 0xFFC0D0E0F00A0B0C ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 16 )))
(local.set $expected ( v128.const i64x2 0 0x00 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 32 )))
(local.set $expected ( v128.const i64x2 0 0x00 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 128 )))
(local.set $expected ( v128.const i64x2 1 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 256 )))
(local.set $expected ( v128.const i64x2 1 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 -9223372036854775808 -2147483648 ) ( i32.const 65 )))
(local.set $expected ( v128.const i64x2 13835058055282163712 18446744072635809792 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 0x0C 0x0D ) ( i32.const 65 )))
(local.set $expected ( v128.const i64x2 0x06 0x06 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 129 )))
(local.set $expected ( v128.const i64x2 0 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 257 )))
(local.set $expected ( v128.const i64x2 0 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 513 )))
(local.set $expected ( v128.const i64x2 0 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s" (func $f (param v128 i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F ) ( i32.const 514 )))
(local.set $expected ( v128.const i64x2 0 0x03 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl_1" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F )))
(local.set $expected ( v128.const i64x2 2 0x1E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u_64" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F )))
(local.set $expected ( v128.const i64x2 1 0x0F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s_65" (func $f (param v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i64x2 1 0x0F )))
(local.set $expected ( v128.const i64x2 0 0x07 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "i8x16.shl-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.shl ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i8x16.shr_s-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.shr_s ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i8x16.shr_u-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.shr_u ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i16x8.shl-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.shl ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i16x8.shr_s-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.shr_s ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i16x8.shr_u-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.shr_u ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i32x4.shl-in-block" ) ( block ( drop ( block ( result v128 ) ( i32x4.shl ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i32x4.shr_s-in-block" ) ( block ( drop ( block ( result v128 ) ( i32x4.shr_s ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i32x4.shr_u-in-block" ) ( block ( drop ( block ( result v128 ) ( i32x4.shr_u ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i64x2.shl-in-block" ) ( block ( drop ( block ( result v128 ) ( i64x2.shl ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i64x2.shr_s-in-block" ) ( block ( drop ( block ( result v128 ) ( i64x2.shr_s ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "i64x2.shr_u-in-block" ) ( block ( drop ( block ( result v128 ) ( i64x2.shr_u ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( i32.const 1 ) ) ) ) ) ) ( func ( export "nested-i8x16.shl" ) ( drop ( i8x16.shl ( i8x16.shl ( i8x16.shl ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i8x16.shr_s" ) ( drop ( i8x16.shr_s ( i8x16.shr_s ( i8x16.shr_s ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i8x16.shr_u" ) ( drop ( i8x16.shr_u ( i8x16.shr_u ( i8x16.shr_u ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i16x8.shl" ) ( drop ( i16x8.shl ( i16x8.shl ( i16x8.shl ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i16x8.shr_s" ) ( drop ( i16x8.shr_s ( i16x8.shr_s ( i16x8.shr_s ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i16x8.shr_u" ) ( drop ( i16x8.shr_u ( i16x8.shr_u ( i16x8.shr_u ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i32x4.shl" ) ( drop ( i32x4.shl ( i32x4.shl ( i32x4.shl ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i32x4.shr_s" ) ( drop ( i32x4.shr_s ( i32x4.shr_s ( i32x4.shr_s ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i32x4.shr_u" ) ( drop ( i32x4.shr_u ( i32x4.shr_u ( i32x4.shr_u ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i64x2.shl" ) ( drop ( i64x2.shl ( i64x2.shl ( i64x2.shl ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i64x2.shr_s" ) ( drop ( i64x2.shr_s ( i64x2.shr_s ( i64x2.shr_s ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) ( func ( export "nested-i64x2.shr_u" ) ( drop ( i64x2.shr_u ( i64x2.shr_u ( i64x2.shr_u ( v128.load ( i32.const 0 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ( i32.const 1 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shl-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_s-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i8x16.shr_u-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shl-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_s-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i16x8.shr_u-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shl-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_s-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i32x4.shr_u-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shl-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_s-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "i64x2.shr_u-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i8x16.shl" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i8x16.shr_s" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i8x16.shr_u" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i16x8.shl" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i16x8.shr_s" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i16x8.shr_u" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i32x4.shl" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i32x4.shr_s" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i32x4.shr_u" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i64x2.shl" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i64x2.shr_s" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-i64x2.shr_u" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.shl ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.shr_s ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.shr_u ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.shl ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.shr_s ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.shr_u ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.shl ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.shr_s ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i32x4.shr_u ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.shl ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.shr_s ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i64x2.shr_u ( i32.const 0 ) ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i8x16.shl_s (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i8x16.shl_r (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i8x16.shr   (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i16x8.shl_s (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i16x8.shl_r (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i16x8.shr   (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i32x4.shl_s (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i32x4.shl_r (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i32x4.shr   (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.shl_s (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.shl_r (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (i64x2.shr   (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f32x4.shl   (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f32x4.shr_s (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (result v128) (f32x4.shr_u (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.shl-1st-arg-empty ( result v128 ) ( i8x16.shl ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.shl-last-arg-empty ( result v128 ) ( i8x16.shl ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.shl-arg-empty ( result v128 ) ( i8x16.shl ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.shr_u-1st-arg-empty ( result v128 ) ( i16x8.shr_u ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.shr_u-last-arg-empty ( result v128 ) ( i16x8.shr_u ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.shr_u-arg-empty ( result v128 ) ( i16x8.shr_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.shr_s-1st-arg-empty ( result v128 ) ( i32x4.shr_s ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.shr_s-last-arg-empty ( result v128 ) ( i32x4.shr_s ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i32x4.shr_s-arg-empty ( result v128 ) ( i32x4.shr_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.shl-1st-arg-empty ( result v128 ) ( i64x2.shl ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.shr_u-last-arg-empty ( result v128 ) ( i64x2.shr_u ( v128.const i64x2 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i64x2.shr_s-arg-empty ( result v128 ) ( i64x2.shr_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)

