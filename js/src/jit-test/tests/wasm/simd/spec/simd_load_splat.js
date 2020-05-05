var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( data ( i32.const 0 ) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A\\0B\\0C\\0D\\0E\\0F" ) ( data ( i32.const 65520 ) "\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1A\\1B\\1C\\1D\\1E\\1F" ) ( func ( export "v8x16.load_splat" ) ( param $address i32 ) ( result v128 ) ( v8x16.load_splat ( local.get $address ) ) ) ( func ( export "v16x8.load_splat" ) ( param $address i32 ) ( result v128 ) ( v16x8.load_splat ( local.get $address ) ) ) ( func ( export "v32x4.load_splat" ) ( param $address i32 ) ( result v128 ) ( v32x4.load_splat ( local.get $address ) ) ) ( func ( export "v64x2.load_splat" ) ( param $address i32 ) ( result v128 ) ( v64x2.load_splat ( local.get $address ) ) ) ( func ( export "v8x16.offset0" ) ( param $address i32 ) ( result v128 ) ( v8x16.load_splat offset=0 ( local.get $address ) ) ) ( func ( export "v8x16.align1" ) ( param $address i32 ) ( result v128 ) ( v8x16.load_splat align=1 ( local.get $address ) ) ) ( func ( export "v8x16.offset1_align1" ) ( param $address i32 ) ( result v128 ) ( v8x16.load_splat offset=1 align=1 ( local.get $address ) ) ) ( func ( export "v8x16.offset2_align1" ) ( param $address i32 ) ( result v128 ) ( v8x16.load_splat offset=2 align=1 ( local.get $address ) ) ) ( func ( export "v8x16.offset15_align1" ) ( param $address i32 ) ( result v128 ) ( v8x16.load_splat offset=15 align=1 ( local.get $address ) ) ) ( func ( export "v16x8.offset0" ) ( param $address i32 ) ( result v128 ) ( v16x8.load_splat offset=0 ( local.get $address ) ) ) ( func ( export "v16x8.align1" ) ( param $address i32 ) ( result v128 ) ( v16x8.load_splat align=1 ( local.get $address ) ) ) ( func ( export "v16x8.offset1_align1" ) ( param $address i32 ) ( result v128 ) ( v16x8.load_splat offset=1 align=1 ( local.get $address ) ) ) ( func ( export "v16x8.offset2_align1" ) ( param $address i32 ) ( result v128 ) ( v16x8.load_splat offset=2 align=1 ( local.get $address ) ) ) ( func ( export "v16x8.offset15_align2" ) ( param $address i32 ) ( result v128 ) ( v16x8.load_splat offset=15 align=2 ( local.get $address ) ) ) ( func ( export "v32x4.offset0" ) ( param $address i32 ) ( result v128 ) ( v32x4.load_splat offset=0 ( local.get $address ) ) ) ( func ( export "v32x4.align1" ) ( param $address i32 ) ( result v128 ) ( v32x4.load_splat align=1 ( local.get $address ) ) ) ( func ( export "v32x4.offset1_align1" ) ( param $address i32 ) ( result v128 ) ( v32x4.load_splat offset=1 align=1 ( local.get $address ) ) ) ( func ( export "v32x4.offset2_align2" ) ( param $address i32 ) ( result v128 ) ( v32x4.load_splat offset=2 align=2 ( local.get $address ) ) ) ( func ( export "v32x4.offset15_align4" ) ( param $address i32 ) ( result v128 ) ( v32x4.load_splat offset=15 align=4 ( local.get $address ) ) ) ( func ( export "v64x2.offset0" ) ( param $address i32 ) ( result v128 ) ( v64x2.load_splat offset=0 ( local.get $address ) ) ) ( func ( export "v64x2.align1" ) ( param $address i32 ) ( result v128 ) ( v64x2.load_splat align=1 ( local.get $address ) ) ) ( func ( export "v64x2.offset1_align2" ) ( param $address i32 ) ( result v128 ) ( v64x2.load_splat offset=1 align=2 ( local.get $address ) ) ) ( func ( export "v64x2.offset2_align4" ) ( param $address i32 ) ( result v128 ) ( v64x2.load_splat offset=2 align=4 ( local.get $address ) ) ) ( func ( export "v64x2.offset15_align8" ) ( param $address i32 ) ( result v128 ) ( v64x2.load_splat offset=15 align=8 ( local.get $address ) ) ) ( func ( export "v8x16.offset65536" ) ( param $address i32 ) ( result v128 ) ( v8x16.load_splat offset=65536 ( local.get $address ) ) ) ( func ( export "v16x8.offset65535" ) ( param $address i32 ) ( result v128 ) ( v16x8.load_splat offset=65535 ( local.get $address ) ) ) ( func ( export "v32x4.offset65533" ) ( param $address i32 ) ( result v128 ) ( v32x4.load_splat offset=65533 ( local.get $address ) ) ) ( func ( export "v64x2.offset65529" ) ( param $address i32 ) ( result v128 ) ( v64x2.load_splat offset=65529 ( local.get $address ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.load_splat" (func $f (param i32) (result v128)))
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
  (import "" "v8x16.load_splat" (func $f (param i32) (result v128)))
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
  (import "" "v8x16.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 2 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 3 )))
(local.set $expected ( v128.const i8x16 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65535 )))
(local.set $expected ( v128.const i8x16 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 4 )))
(local.set $expected ( v128.const i16x8 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 5 )))
(local.set $expected ( v128.const i16x8 0x0605 0x0605 0x0605 0x0605 0x0605 0x0605 0x0605 0x0605 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 6 )))
(local.set $expected ( v128.const i16x8 0x0706 0x0706 0x0706 0x0706 0x0706 0x0706 0x0706 0x0706 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 7 )))
(local.set $expected ( v128.const i16x8 0x0807 0x0807 0x0807 0x0807 0x0807 0x0807 0x0807 0x0807 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65534 )))
(local.set $expected ( v128.const i16x8 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 8 )))
(local.set $expected ( v128.const i32x4 0x0B0A0908 0x0B0A0908 0x0B0A0908 0x0B0A0908 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 9 )))
(local.set $expected ( v128.const i32x4 0x0C0B0A09 0x0C0B0A09 0x0C0B0A09 0x0C0B0A09 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 10 )))
(local.set $expected ( v128.const i32x4 0x0D0C0B0A 0x0D0C0B0A 0x0D0C0B0A 0x0D0C0B0A ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 11 )))
(local.set $expected ( v128.const i32x4 0x0E0D0C0B 0x0E0D0C0B 0x0E0D0C0B 0x0E0D0C0B ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65532 )))
(local.set $expected ( v128.const i32x4 0x1F1E1D1C 0x1F1E1D1C 0x1F1E1D1C 0x1F1E1D1C ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 12 )))
(local.set $expected ( v128.const i64x2 0x000000000F0E0D0C 0x000000000F0E0D0C ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 13 )))
(local.set $expected ( v128.const i64x2 0x00000000000F0E0D 0x00000000000F0E0D ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 14 )))
(local.set $expected ( v128.const i64x2 0x0000000000000F0E 0x0000000000000F0E ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 15 )))
(local.set $expected ( v128.const i64x2 0x000000000000000F 0x000000000000000F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65528 )))
(local.set $expected ( v128.const i64x2 0x1F1E1D1C1B1A1918 0x1F1E1D1C1B1A1918 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset0" (func $f (param i32) (result v128)))
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
  (import "" "v8x16.align1" (func $f (param i32) (result v128)))
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
  (import "" "v8x16.offset1_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset2_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset15_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset0" (func $f (param i32) (result v128)))
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
  (import "" "v8x16.align1" (func $f (param i32) (result v128)))
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
  (import "" "v8x16.offset1_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset2_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset15_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65535 )))
(local.set $expected ( v128.const i8x16 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65535 )))
(local.set $expected ( v128.const i8x16 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100 0x0100 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset1_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset2_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset15_align2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x000F 0x000F 0x000F 0x000F 0x000F 0x000F 0x000F 0x000F ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset1_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302 0x0302 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset2_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 0x0403 0x0403 0x0403 0x0403 0x0403 0x0403 0x0403 0x0403 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset15_align2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i16x8 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65534 )))
(local.set $expected ( v128.const i16x8 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65534 )))
(local.set $expected ( v128.const i16x8 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E 0x1F1E ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x03020100 0x03020100 0x03020100 0x03020100 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x03020100 0x03020100 0x03020100 0x03020100 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset1_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x04030201 0x04030201 0x04030201 0x04030201 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset2_align2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x05040302 0x05040302 0x05040302 0x05040302 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset15_align4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x0000000F 0x0000000F 0x0000000F 0x0000000F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 0x04030201 0x04030201 0x04030201 0x04030201 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 0x04030201 0x04030201 0x04030201 0x04030201 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset1_align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 0x05040302 0x05040302 0x05040302 0x05040302 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset2_align2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 0x06050403 0x06050403 0x06050403 0x06050403 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset15_align4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65532 )))
(local.set $expected ( v128.const i32x4 0x1F1E1D1C 0x1F1E1D1C 0x1F1E1D1C 0x1F1E1D1C ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65532 )))
(local.set $expected ( v128.const i32x4 0x1F1E1D1C 0x1F1E1D1C 0x1F1E1D1C 0x1F1E1D1C ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i64x2 0x0706050403020100 0x0706050403020100 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i64x2 0x0706050403020100 0x0706050403020100 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset1_align2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i64x2 0x0807060504030201 0x0807060504030201 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset2_align4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i64x2 0x0908070605040302 0x0908070605040302 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset15_align8" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i64x2 0x000000000000000F 0x000000000000000F ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i64x2 0x0807060504030201 0x0807060504030201 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i64x2 0x0807060504030201 0x0807060504030201 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset1_align2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i64x2 0x0908070605040302 0x0908070605040302 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset2_align4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i64x2 0x0A09080706050403 0x0A09080706050403 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset15_align8" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 1 )))
(local.set $expected ( v128.const i64x2 0x0000000000000000 0x0000000000000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset0" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65528 )))
(local.set $expected ( v128.const i64x2 0x1F1E1D1C1B1A1918 0x1F1E1D1C1B1A1918 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.align1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65528 )))
(local.set $expected ( v128.const i64x2 0x1F1E1D1C1B1A1918 0x1F1E1D1C1B1A1918 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.load_splat" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const -1 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const -1 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const -1 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const -1 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.load_splat" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65536 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65535 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65533 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65529 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset1_align1" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65535 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset2_align1" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65535 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset15_align1" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65535 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset1_align1" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65534 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset2_align1" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65534 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset15_align2" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65534 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset1_align1" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65532 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset2_align2" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65532 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset15_align4" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65532 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset1_align2" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65528 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset2_align4" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65528 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset15_align8" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65528 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset65536" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 0 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset65535" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 0 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset65533" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 0 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset65529" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 0 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.offset65536" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 1 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.offset65535" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 1 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.offset65533" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 1 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.offset65529" (func $f (param $address i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 1 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( data ( i32.const 0 ) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0A" ) ( func ( export "v8x16.load_splat-in-block" ) ( result v128 ) ( block ( result v128 ) ( block ( result v128 ) ( v8x16.load_splat ( i32.const 0 ) ) ) ) ) ( func ( export "v16x8.load_splat-in-block" ) ( result v128 ) ( block ( result v128 ) ( block ( result v128 ) ( v16x8.load_splat ( i32.const 1 ) ) ) ) ) ( func ( export "v32x4.load_splat-in-block" ) ( result v128 ) ( block ( result v128 ) ( block ( result v128 ) ( v32x4.load_splat ( i32.const 2 ) ) ) ) ) ( func ( export "v64x2.load_splat-in-block" ) ( result v128 ) ( block ( result v128 ) ( block ( result v128 ) ( v64x2.load_splat ( i32.const 9 ) ) ) ) ) ( func ( export "v8x16.load_splat-as-br-value" ) ( result v128 ) ( block ( result v128 ) ( br 0 ( v8x16.load_splat ( i32.const 3 ) ) ) ) ) ( func ( export "v16x8.load_splat-as-br-value" ) ( result v128 ) ( block ( result v128 ) ( br 0 ( v16x8.load_splat ( i32.const 4 ) ) ) ) ) ( func ( export "v32x4.load_splat-as-br-value" ) ( result v128 ) ( block ( result v128 ) ( br 0 ( v32x4.load_splat ( i32.const 5 ) ) ) ) ) ( func ( export "v64x2.load_splat-as-br-value" ) ( result v128 ) ( block ( result v128 ) ( br 0 ( v64x2.load_splat ( i32.const 10 ) ) ) ) ) ( func ( export "v8x16.load_splat-extract_lane_s-operand" ) ( result i32 ) ( i8x16.extract_lane_s 0 ( v8x16.load_splat ( i32.const 6 ) ) ) ) ( func ( export "v16x8.load_splat-extract_lane_s-operand" ) ( result i32 ) ( i8x16.extract_lane_s 0 ( v16x8.load_splat ( i32.const 7 ) ) ) ) ( func ( export "v32x4.load_splat-extract_lane_s-operand" ) ( result i32 ) ( i8x16.extract_lane_s 0 ( v32x4.load_splat ( i32.const 8 ) ) ) ) ( func ( export "v64x2.load_splat-extract_lane_s-operand" ) ( result i32 ) ( i8x16.extract_lane_s 0 ( v64x2.load_splat ( i32.const 11 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.load_splat-in-block" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat-in-block" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i16x8 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 0x0201 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat-in-block" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 0x05040302 0x05040302 0x05040302 0x05040302 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat-in-block" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i64x2 0x0000000000000A09 0x0000000000000A09 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.load_splat-as-br-value" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat-as-br-value" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i16x8 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 0x0504 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat-as-br-value" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 0x08070605 0x08070605 0x08070605 0x08070605 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat-as-br-value" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i64x2 0x000000000000000A 0x000000000000000A ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v8x16.load_splat-extract_lane_s-operand" (func $f (param ) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ))
(local.set $expected ( i32.const 6 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v16x8.load_splat-extract_lane_s-operand" (func $f (param ) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ))
(local.set $expected ( i32.const 7 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v32x4.load_splat-extract_lane_s-operand" (func $f (param ) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ))
(local.set $expected ( i32.const 8 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v64x2.load_splat-extract_lane_s-operand" (func $f (param ) (result i32)))
  (func (export "run") (result i32) 
(local $result i32)
(local $expected i32)
(local $cmpresult i32)

(local.set $result (call $f ))
(local.set $expected ( i32.const 0 ))

(local.set $cmpresult (i32.eq (local.get $result) (local.get $expected)))
(local.get $cmpresult)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func ( result v128 ) ( v8x16.load_splat ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func ( result v128 ) ( v16x8.load_splat ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func ( result v128 ) ( v32x4.load_splat ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func ( result v128 ) ( v64x2.load_splat ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (drop (i8x16.load_splat (i32.const 0)))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (drop (i16x8.load_splat (i32.const 0)))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (drop (i32x4.load_splat (i32.const 0)))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1) (func (drop (i64x2.load_splat (i32.const 0)))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func $v8x16.load_splat-arg-empty ( result v128 ) ( v8x16.load_splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func $v16x8.load_splat-arg-empty ( result v128 ) ( v16x8.load_splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func $v32x4.load_splat-arg-empty ( result v128 ) ( v32x4.load_splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func $v64x2.load_splat-arg-empty ( result v128 ) ( v64x2.load_splat ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)

