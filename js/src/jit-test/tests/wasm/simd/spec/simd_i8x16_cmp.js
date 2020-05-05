var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( func ( export "eq" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.eq ( local.get $x ) ( local.get $y ) ) ) ( func ( export "ne" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.ne ( local.get $x ) ( local.get $y ) ) ) ( func ( export "lt_s" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.lt_s ( local.get $x ) ( local.get $y ) ) ) ( func ( export "lt_u" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.lt_u ( local.get $x ) ( local.get $y ) ) ) ( func ( export "le_s" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.le_s ( local.get $x ) ( local.get $y ) ) ) ( func ( export "le_u" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.le_u ( local.get $x ) ( local.get $y ) ) ) ( func ( export "gt_s" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.gt_s ( local.get $x ) ( local.get $y ) ) ) ( func ( export "gt_u" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.gt_u ( local.get $x ) ( local.get $y ) ) ) ( func ( export "ge_s" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.ge_s ( local.get $x ) ( local.get $y ) ) ) ( func ( export "ge_u" ) ( param $x v128 ) ( param $y v128 ) ( result v128 ) ( i8x16.ge_u ( local.get $x ) ( local.get $y ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "eq" (func $f (param v128 v128) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 -1 -1 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 -1 -1 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 -1 0 -1 0 -1 -1 -1 -1 -1 0 -1 0 -1 0 -1 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 -1 0 0 0 -1 -1 -1 -1 -1 0 0 0 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 0 0 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 0 0 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 0 -1 0 -1 0 0 0 0 0 -1 0 -1 0 -1 0 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 0 -1 -1 -1 0 0 0 0 0 -1 -1 -1 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 -1 -1 0 -1 0 -1 0 0 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 -1 -1 -1 0 0 0 0 0 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 -1 -1 -1 0 0 0 0 0 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 0 -1 0 -1 0 0 0 0 0 0 0 0 0 -1 0 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 0 -1 0 -1 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 0 0 0 0 0 -1 -1 -1 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 0 0 0 0 0 -1 -1 -1 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 0 -1 0 -1 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 -1 -1 0 -1 0 -1 0 0 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 -1 -1 -1 -1 -1 0 0 0 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 -1 -1 -1 -1 -1 0 0 0 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 0 -1 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 0 -1 0 -1 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 0 0 0 -1 -1 -1 -1 -1 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 0 0 0 -1 -1 -1 -1 -1 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 0 -1 0 -1 0 -1 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 0 0 -1 0 -1 0 -1 -1 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 0 0 0 0 0 -1 -1 -1 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 0 0 0 0 0 -1 -1 -1 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 -1 0 -1 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 -1 -1 -1 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 -1 0 -1 0 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 -1 -1 -1 0 0 0 0 0 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 -1 -1 -1 0 0 0 0 0 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 -1 0 -1 0 -1 0 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 -1 -1 -1 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 0 0 0 -1 0 -1 0 -1 -1 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 0 0 0 -1 -1 -1 -1 -1 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 0 0 0 -1 -1 -1 -1 -1 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 -1 0 -1 0 -1 -1 -1 -1 -1 -1 -1 -1 -1 0 -1 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 -1 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 -1 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 0x80 ) ( v128.const i8x16 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 -128 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 ) ( v128.const i8x16 255 255 255 255 255 255 255 255 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 ) ( v128.const i8x16 0 0 0 0 0 0 0 0 255 255 255 255 255 255 255 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 -0 0 1 2 127 128 253 254 255 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0xc3 0x00 0x00 0xfe 0xc2 0x00 0x00 0x80 0xbf 0x00 0x00 0x00 0x00 ) ( v128.const f32x4 -128.0 -127.0 -1.0 0.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x80 0x3f 0x00 0x00 0xfe 0x42 0x00 0x00 0x00 0x43 0x00 0x00 0x7f 0x43 ) ( v128.const f32x4 1.0 127.0 128.0 255.0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F 0x0F ) ( v128.const i8x16 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 0xF0 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x09 0x10 0x11 0x12 0x0A 0x0B 0x1A 0x1B 0xAA 0xAB 0xFF ) ( v128.const i8x16 0xFF 0xAB 0xAA 0x1B 0x1A 0x0B 0x0A 0x12 0x11 0x10 0x09 0x04 0x03 0x02 0x01 0x00 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 -1 0 -1 0 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x80 0x81 0x82 0x83 0xFD 0xFE 0xFF 0x00 0x00 0x01 0x02 0x7F 0x80 0xFD 0xFE 0xFF ) ( v128.const i8x16 255 254 253 128 127 2 1 0 0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 -1 -1 -1 -1 -1 0 0 0 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 128 129 130 131 253 254 255 -0 0 1 2 127 128 253 254 255 ) ( v128.const i8x16 255 254 253 128 127 2 1 0 -0 -1 -2 -3 -125 -126 -127 -128 )))
(local.set $expected ( v128.const i8x16 0 0 0 -1 -1 -1 -1 -1 -1 0 0 0 0 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i16x8 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF 0xFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i16x8 65535 65535 65535 65535 65535 65535 65535 65535 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i16x8 0 0 0 0 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x0B0A 0x0D0C 0x0F0E )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i16x8 33152 33666 65277 255 256 32514 64896 65534 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i16x8 -128 -128 0 0 1 1 255 255 )))
(local.set $expected ( v128.const i8x16 -1 0 -1 0 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i16x8 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA 0xAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF ) ( v128.const i32x4 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 ) ( v128.const i32x4 4294967295 4294967295 4294967295 4294967295 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ( v128.const i32x4 0 0 0 0 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F ) ( v128.const i32x4 0x03020100 0x07060504 0x0B0A0908 0x0F0E0D0C )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -127 -126 -125 -3 -2 -1 0 0 1 2 127 128 253 254 255 ) ( v128.const i32x4 2206368128 16776957 2130837760 4294901120 )))
(local.set $expected ( v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 -128 -128 -128 -128 0 0 0 0 1 1 1 1 255 255 255 255 ) ( v128.const i32x4 -128 0 1 255 )))
(local.set $expected ( v128.const i8x16 -1 0 0 0 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
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

(local.set $result (call $f ( v128.const i8x16 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 0x55 ) ( v128.const i32x4 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA 0xAAAAAAAA )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.eq ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.ge_s ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.ge_u ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.gt_s ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.gt_u ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.le_s ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.le_u ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.lt_s ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.lt_u ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func ( result v128 ) ( i8x16.ne ( i32.const 0 ) ( f32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "eq-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.eq ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "ne-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.ne ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "lt_s-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.lt_s ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "le_u-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.le_u ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "gt_u-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.gt_u ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "ge_s-in-block" ) ( block ( drop ( block ( result v128 ) ( i8x16.ge_s ( block ( result v128 ) ( v128.load ( i32.const 0 ) ) ) ( block ( result v128 ) ( v128.load ( i32.const 1 ) ) ) ) ) ) ) ) ( func ( export "nested-eq" ) ( drop ( i8x16.eq ( i8x16.eq ( i8x16.eq ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.eq ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i8x16.eq ( i8x16.eq ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.eq ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-ne" ) ( drop ( i8x16.ne ( i8x16.ne ( i8x16.ne ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.ne ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i8x16.ne ( i8x16.ne ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.ne ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-lt_s" ) ( drop ( i8x16.lt_s ( i8x16.lt_s ( i8x16.lt_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.lt_s ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i8x16.lt_s ( i8x16.lt_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.lt_s ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-le_u" ) ( drop ( i8x16.le_u ( i8x16.le_u ( i8x16.le_u ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.le_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i8x16.le_u ( i8x16.le_u ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.le_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-gt_u" ) ( drop ( i8x16.gt_u ( i8x16.gt_u ( i8x16.gt_u ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.gt_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i8x16.gt_u ( i8x16.gt_u ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.gt_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "nested-ge_s" ) ( drop ( i8x16.ge_s ( i8x16.ge_s ( i8x16.ge_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.ge_s ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i8x16.ge_s ( i8x16.ge_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.ge_s ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) ( func ( export "as-param" ) ( drop ( i8x16.ge_u ( i8x16.eq ( i8x16.lt_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.le_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ( i8x16.ne ( i8x16.gt_s ( v128.load ( i32.const 0 ) ) ( v128.load ( i32.const 1 ) ) ) ( i8x16.lt_u ( v128.load ( i32.const 2 ) ) ( v128.load ( i32.const 3 ) ) ) ) ) ) ) )
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
( module ( func $i8x16.eq-1st-arg-empty ( result v128 ) ( i8x16.eq ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.eq-arg-empty ( result v128 ) ( i8x16.eq ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.ne-1st-arg-empty ( result v128 ) ( i8x16.ne ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.ne-arg-empty ( result v128 ) ( i8x16.ne ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.lt_s-1st-arg-empty ( result v128 ) ( i8x16.lt_s ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.lt_s-arg-empty ( result v128 ) ( i8x16.lt_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.lt_u-1st-arg-empty ( result v128 ) ( i8x16.lt_u ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.lt_u-arg-empty ( result v128 ) ( i8x16.lt_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.le_s-1st-arg-empty ( result v128 ) ( i8x16.le_s ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.le_s-arg-empty ( result v128 ) ( i8x16.le_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.le_u-1st-arg-empty ( result v128 ) ( i8x16.le_u ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.le_u-arg-empty ( result v128 ) ( i8x16.le_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.gt_s-1st-arg-empty ( result v128 ) ( i8x16.gt_s ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.gt_s-arg-empty ( result v128 ) ( i8x16.gt_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.gt_u-1st-arg-empty ( result v128 ) ( i8x16.gt_u ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.gt_u-arg-empty ( result v128 ) ( i8x16.gt_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.ge_s-1st-arg-empty ( result v128 ) ( i8x16.ge_s ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.ge_s-arg-empty ( result v128 ) ( i8x16.ge_s ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.ge_u-1st-arg-empty ( result v128 ) ( i8x16.ge_u ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( func $i8x16.ge_u-arg-empty ( result v128 ) ( i8x16.ge_u ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)

