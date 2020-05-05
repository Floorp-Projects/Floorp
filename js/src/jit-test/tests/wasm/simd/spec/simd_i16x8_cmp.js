var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "eq" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.eq ( local.get $x ) ( local.get $y ) ) ) ( func ( export "ne" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.ne ( local.get $x ) ( local.get $y ) ) ) ( func ( export "lt_s" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.lt_s ( local.get $x ) ( local.get $y ) ) ) ( func ( export "lt_u" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.lt_u ( local.get $x ) ( local.get $y ) ) ) ( func ( export "le_s" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.le_s ( local.get $x ) ( local.get $y ) ) ) ( func ( export "le_u" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.le_u ( local.get $x ) ( local.get $y ) ) ) ( func ( export "gt_s" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.gt_s ( local.get $x ) ( local.get $y ) ) ) ( func ( export "gt_u" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.gt_u ( local.get $x ) ( local.get $y ) ) ) ( func ( export "ge_s" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.ge_s ( local.get $x ) ( local.get $y ) ) ) ( func ( export "ge_u" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i16x8.ge_u ( local.get $x ) ( local.get $y ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 ) ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 65534 -1 -0 0 1 2 65535 ) ( v128.const i16x8 32768 -2 -1 -0 0 1 2 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8001 0x8002 0x8003 0x8004 0x8005 0x8006 0x8007 ) ( v128.const i16x8 32775 32774 32773 32772 32771 32770 32769 32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32769 65534 65535 0 -1 -32767 -32768 ) ( v128.const i16x8 -32768 -32767 -1 0 65535 65534 32769 32768 )))
(local.set $expected ( v128.const i16x8 -1 -1 0 0 0 0 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 0 0 1 1 32768 32768 ) ( v128.const i32x4 65535 0 1 32768 )))
(local.set $expected ( v128.const i16x8 -1 0 -1 -1 -1 0 -1 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
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
  (import "" "ne" (func $f (param v128 v128) (result v128)))
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
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
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
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 255 255 255 0 0 0 0 ) ( v128.const i16x8 255 255 255 255 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 255 255 255 255 ) ( v128.const i16x8 0 0 0 0 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 32767 -0 0 1 2 65534 65535 ) ( v128.const i16x8 255 32767 0 0 1 2 -2 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8081 0x8283 0xFDFE 0xFF00 0x0001 0x027F 0x80FD 0xFEFF ) ( v128.const i16x8 65279 33021 639 1 65280 65022 33411 32897 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 128 129 130 131 -0 255 32766 32767 ) ( v128.const i16x8 32767 32766 255 -0 131 130 129 28 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i16x8 0 -1 0 0 0 -1 0 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
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
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
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
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
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
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 255 255 255 0 0 0 0 ) ( v128.const i16x8 255 255 255 255 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 255 255 255 255 ) ( v128.const i16x8 0 0 0 0 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 32767 -0 0 1 2 65534 65535 ) ( v128.const i16x8 255 32767 0 0 1 2 -2 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 0 0 -1 0 0 0 0 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8081 0x8283 0xFDFE 0xFF00 0x0001 0x027F 0x80FD 0xFEFF ) ( v128.const i16x8 65279 33021 639 1 65280 65022 33411 32897 )))
(local.set $expected ( v128.const i16x8 -1 0 -1 -1 0 0 -1 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 128 129 130 131 -0 255 32766 32767 ) ( v128.const i16x8 32767 32766 255 -0 131 130 129 28 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 0 -1 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i16x8 0 -1 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
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
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
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
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
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
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 255 255 255 0 0 0 0 ) ( v128.const i16x8 255 255 255 255 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 255 255 255 255 ) ( v128.const i16x8 0 0 0 0 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 255 32767 -0 0 1 2 65534 65535 ) ( v128.const i16x8 255 32767 0 0 1 2 -2 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8081 0x8283 0xFDFE 0xFF00 0x0001 0x027F 0x80FD 0xFEFF ) ( v128.const i16x8 65279 33021 639 1 65280 65022 33411 32897 )))
(local.set $expected ( v128.const i16x8 -1 0 0 0 -1 -1 -1 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 128 129 130 131 -0 255 32766 32767 ) ( v128.const i16x8 32767 32766 255 -0 131 130 129 28 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 0 -1 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i16x8 0 -1 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 ) ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 65534 -1 -0 0 1 2 65535 ) ( v128.const i16x8 32768 -2 -1 -0 0 1 2 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 0 0 -1 0 0 0 0 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8001 0x8002 0x8003 0x8004 0x8005 0x8006 0x8007 ) ( v128.const i16x8 32775 32774 32773 32772 32771 32770 32769 32768 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32769 65534 65535 0 -1 -32767 -32768 ) ( v128.const i16x8 -32768 -32767 -1 0 65535 65534 32769 32768 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 -1 -1 -1 -1 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 0 -1 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 ) ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 65534 -1 -0 0 1 2 65535 ) ( v128.const i16x8 32768 -2 -1 -0 0 1 2 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8001 0x8002 0x8003 0x8004 0x8005 0x8006 0x8007 ) ( v128.const i16x8 32775 32774 32773 32772 32771 32770 32769 32768 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32769 65534 65535 0 -1 -32767 -32768 ) ( v128.const i16x8 -32768 -32767 -1 0 65535 65534 32769 32768 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 0 -1 0 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 0 -1 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_edcb 0x0_edcb 0x0_edcb 0x0_edcb 0x0_edcb 0x0_edcb 0x0_edcb 0x0_edcb ) ( v128.const i16x8 -0x1234 -0x1234 -0x1234 -0x1234 -0x1234 -0x1234 -0x1234 -0x1234 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
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
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
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
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
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
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
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
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 ) ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 65534 -1 -0 0 1 2 65535 ) ( v128.const i16x8 32768 -2 -1 -0 0 1 2 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 -1 -1 0 -1 -1 -1 -1 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8001 0x8002 0x8003 0x8004 0x8005 0x8006 0x8007 ) ( v128.const i16x8 32775 32774 32773 32772 32771 32770 32769 32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32769 65534 65535 0 -1 -32767 -32768 ) ( v128.const i16x8 -32768 -32767 -1 0 65535 65534 32769 32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 0 0 0 0 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 0 0 1 1 32768 32768 ) ( v128.const i32x4 65535 0 1 32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 -1 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 ) ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 65534 -1 -0 0 1 2 65535 ) ( v128.const i16x8 32768 -2 -1 -0 0 1 2 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 0 0 0 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8001 0x8002 0x8003 0x8004 0x8005 0x8006 0x8007 ) ( v128.const i16x8 32775 32774 32773 32772 32771 32770 32769 32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32769 65534 65535 0 -1 -32767 -32768 ) ( v128.const i16x8 -32768 -32767 -1 0 65535 65534 32769 32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 -1 0 -1 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i8x16 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA 0xAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 -1 0 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 ) ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 65534 -1 -0 0 1 2 65535 ) ( v128.const i16x8 32768 -2 -1 -0 0 1 2 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 -1 -1 0 -1 -1 -1 -1 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8001 0x8002 0x8003 0x8004 0x8005 0x8006 0x8007 ) ( v128.const i16x8 32775 32774 32773 32772 32771 32770 32769 32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32769 65534 65535 0 -1 -32767 -32768 ) ( v128.const i16x8 -32768 -32767 -1 0 65535 65534 32769 32768 )))
(local.set $expected ( v128.const i16x8 -1 -1 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 0 0 1 1 32768 32768 ) ( v128.const i32x4 65535 0 1 32768 )))
(local.set $expected ( v128.const i16x8 -1 0 -1 -1 -1 -1 -1 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB ) ( v128.const i16x8 0x0100 0x0302 0x0904 0x1110 0x0A12 0x1A0B 0xAA1B 0xFFAB )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 32896 32896 32896 32896 32896 32896 32896 32896 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 0x8080 ) ( v128.const i16x8 -32640 -32640 -32640 -32640 -32640 -32640 -32640 -32640 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8180 0x8382 0xFEFD 0x00FF 0x0100 0x7F02 0xFD80 0xFFFE ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 ) ( v128.const i16x8 65535 65535 65535 65535 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 ) ( v128.const i16x8 0 0 0 0 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -32768 65534 -1 -0 0 1 2 65535 ) ( v128.const i16x8 32768 -2 -1 -0 0 1 2 -1 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0xc300 0x0000 0xc2fe 0x0000 0xbf80 0x0000 0x0000 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x3f80 0x0000 0x42fe 0x0000 0x4300 0x0000 0x437f ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F 0x0F0F ) ( v128.const i16x8 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 0xF0F0 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0x0000 0x0000 0x0000 0x0000 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0001 0x0203 0x0409 0x1011 0x120A 0x0B1A 0x1BAA 0xABFF ) ( v128.const i16x8 0xFFAB 0xAA1B 0x1A0B 0x0A12 0x1110 0x0904 0x0302 0x0100 )))
(local.set $expected ( v128.const i16x8 0 0 0 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x8000 0x8001 0x8002 0x8003 0x8004 0x8005 0x8006 0x8007 ) ( v128.const i16x8 32775 32774 32773 32772 32771 32770 32769 32768 )))
(local.set $expected ( v128.const i16x8 0 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 32768 32769 65534 65535 0 -1 -32767 -32768 ) ( v128.const i16x8 -32768 -32767 -1 0 65535 65534 32769 32768 )))
(local.set $expected ( v128.const i16x8 -1 -1 0 -1 0 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 -128 -128 0 0 1 1 255 255 ) ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA ) ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 65535 65535 0 0 1 1 32768 32768 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 0x5555 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i16x8 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_u" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 )))
(local.set $expected ( v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.eq ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.ge_s ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.ge_u ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.gt_s ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.gt_u ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.le_s ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.le_u ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.lt_s ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.lt_u ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i16x8.ne ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "eq-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.eq ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "ne-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.ne ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "lt_s-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.lt_s ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "le_u-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.le_u ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "gt_u-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.gt_u ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "ge_s-in-block" ) ( block ( drop ( block ( result v128 ) ( i16x8.ge_s ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "nested-eq" ) ( drop ( i16x8.eq ( i16x8.eq ( i16x8.eq ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.eq ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i16x8.eq ( i16x8.eq ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.eq ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-ne" ) ( drop ( i16x8.ne ( i16x8.ne ( i16x8.ne ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.ne ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i16x8.ne ( i16x8.ne ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.ne ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-lt_s" ) ( drop ( i16x8.lt_s ( i16x8.lt_s ( i16x8.lt_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.lt_s ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i16x8.lt_s ( i16x8.lt_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.lt_s ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-le_u" ) ( drop ( i16x8.le_u ( i16x8.le_u ( i16x8.le_u ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.le_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i16x8.le_u ( i16x8.le_u ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.le_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-gt_u" ) ( drop ( i16x8.gt_u ( i16x8.gt_u ( i16x8.gt_u ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.gt_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i16x8.gt_u ( i16x8.gt_u ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.gt_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-ge_s" ) ( drop ( i16x8.ge_s ( i16x8.ge_s ( i16x8.ge_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.ge_s ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i16x8.ge_s ( i16x8.ge_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.ge_s ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "as-param" ) ( drop ( i16x8.ge_u ( i16x8.eq ( i16x8.lt_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.le_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i16x8.ne ( i16x8.gt_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i16x8.lt_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ne-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "lt_s-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "le_u-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "gt_u-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "ge_s-in-block" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-eq" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-ne" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-lt_s" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-le_u" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-gt_u" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "nested-ge_s" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-param" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.eq-1st-arg-empty ( result v128 ) ( i16x8.eq ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.eq-arg-empty ( result v128 ) ( i16x8.eq ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.ne-1st-arg-empty ( result v128 ) ( i16x8.ne ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.ne-arg-empty ( result v128 ) ( i16x8.ne ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.lt_s-1st-arg-empty ( result v128 ) ( i16x8.lt_s ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.lt_s-arg-empty ( result v128 ) ( i16x8.lt_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.lt_u-1st-arg-empty ( result v128 ) ( i16x8.lt_u ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.lt_u-arg-empty ( result v128 ) ( i16x8.lt_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.le_s-1st-arg-empty ( result v128 ) ( i16x8.le_s ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.le_s-arg-empty ( result v128 ) ( i16x8.le_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.le_u-1st-arg-empty ( result v128 ) ( i16x8.le_u ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.le_u-arg-empty ( result v128 ) ( i16x8.le_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.gt_s-1st-arg-empty ( result v128 ) ( i16x8.gt_s ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.gt_s-arg-empty ( result v128 ) ( i16x8.gt_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.gt_u-1st-arg-empty ( result v128 ) ( i16x8.gt_u ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.gt_u-arg-empty ( result v128 ) ( i16x8.gt_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.ge_s-1st-arg-empty ( result v128 ) ( i16x8.ge_s ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.ge_s-arg-empty ( result v128 ) ( i16x8.ge_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.ge_u-1st-arg-empty ( result v128 ) ( i16x8.ge_u ( v128.const i16x8 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i16x8.ge_u-arg-empty ( result v128 ) ( i16x8.ge_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)

