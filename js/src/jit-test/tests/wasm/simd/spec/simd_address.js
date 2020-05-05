var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( data ( i32.const 0 ) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\10\\11\\12\\13\\14\\15" ) ( data ( offset ( i32.const 65505 ) ) "\\16\\17\\18\\19\\20\\21\\22\\23\\24\\25\\26\\27\\28\\29\\30\\31" ) ( func ( export "load_data_1" ) ( param $i i32 ) ( result v128 ) ( v128.load offset=0 ( local.get $i ) ) ) ( func ( export "load_data_2" ) ( param $i i32 ) ( result v128 ) ( v128.load align=1 ( local.get $i ) ) ) ( func ( export "load_data_3" ) ( param $i i32 ) ( result v128 ) ( v128.load offset=1 align=1 ( local.get $i ) ) ) ( func ( export "load_data_4" ) ( param $i i32 ) ( result v128 ) ( v128.load offset=2 align=1 ( local.get $i ) ) ) ( func ( export "load_data_5" ) ( param $i i32 ) ( result v128 ) ( v128.load offset=15 align=1 ( local.get $i ) ) ) ( func ( export "store_data_0" ) ( result v128 ) ( v128.store offset=0 ( i32.const 0 ) ( v128.const f32x4 0 1 2 3 ) ) ( v128.load offset=0 ( i32.const 0 ) ) ) ( func ( export "store_data_1" ) ( result v128 ) ( v128.store align=1 ( i32.const 0 ) ( v128.const i32x4 0 1 2 3 ) ) ( v128.load align=1 ( i32.const 0 ) ) ) ( func ( export "store_data_2" ) ( result v128 ) ( v128.store offset=1 align=1 ( i32.const 0 ) ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ) ( v128.load offset=1 align=1 ( i32.const 0 ) ) ) ( func ( export "store_data_3" ) ( result v128 ) ( v128.store offset=2 align=1 ( i32.const 0 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ) ( v128.load offset=2 align=1 ( i32.const 0 ) ) ) ( func ( export "store_data_4" ) ( result v128 ) ( v128.store offset=15 align=1 ( i32.const 0 ) ( v128.const i32x4 0 1 2 3 ) ) ( v128.load offset=15 ( i32.const 0 ) ) ) ( func ( export "store_data_5" ) ( result v128 ) ( v128.store offset=65520 align=1 ( i32.const 0 ) ( v128.const i32x4 0 1 2 3 ) ) ( v128.load offset=65520 ( i32.const 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x03020100 0x07060504 0x11100908 0x15141312 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x03020100 0x07060504 0x11100908 0x15141312 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_3" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x04030201 0x08070605 0x12111009 0x00151413 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x05040302 0x09080706 0x13121110 0x00001514 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_5" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i32x4 0x00000015 0x00000000 0x00000000 0x00000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x1110 0x1312 0x1514 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0100 0x0302 0x0504 0x0706 0x0908 0x1110 0x1312 0x1514 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_3" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0201 0x0403 0x0605 0x0807 0x1009 0x1211 0x1413 0x0015 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0302 0x0504 0x0706 0x0908 0x1110 0x1312 0x1514 0x0000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_5" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i16x8 0x0015 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x10 0x11 0x12 0x13 0x14 0x15 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x10 0x11 0x12 0x13 0x14 0x15 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_3" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x10 0x11 0x12 0x13 0x14 0x15 0x00 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x10 0x11 0x12 0x13 0x14 0x15 0x00 0x00 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_5" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 0 )))
(local.set $expected ( v128.const i8x16 0x15 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i32x4 0x19181716 0x23222120 0x27262524 0x31302928 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i32x4 0x19181716 0x23222120 0x27262524 0x31302928 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_3" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i32x4 0x20191817 0x24232221 0x28272625 0x00313029 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i32x4 0x21201918 0x25242322 0x29282726 0x00003130 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_5" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i32x4 0x00000031 0x00000000 0x00000000 0x00000000 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i16x8 0x1716 0x1918 0x2120 0x2322 0x2524 0x2726 0x2928 0x3130 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i16x8 0x1716 0x1918 0x2120 0x2322 0x2524 0x2726 0x2928 0x3130 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_3" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i16x8 0x1817 0x2019 0x2221 0x2423 0x2625 0x2827 0x3029 0x0031 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i16x8 0x1918 0x2120 0x2322 0x2524 0x2726 0x2928 0x3130 0x0000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_5" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i16x8 0x0031 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_1" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i8x16 0x16 0x17 0x18 0x19 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x30 0x31 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_2" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i8x16 0x16 0x17 0x18 0x19 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x30 0x31 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_3" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i8x16 0x17 0x18 0x19 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x30 0x31 0x00 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_4" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i8x16 0x18 0x19 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x30 0x31 0x00 0x00 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_5" (func $f (param i32) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ( i32.const 65505 )))
(local.set $expected ( v128.const i8x16 0x31 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "load_data_5" (func $f (param $i i32)
                                 (result v128)))
  (func (export "run")
    (call $f ( i32.const 65506 ))
    drop))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "store_data_0" (func $f (param ) (result v128)))
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
  (import "" "store_data_1" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 0 1 2 3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "store_data_2" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i16x8 0 1 2 3 4 5 6 7 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "store_data_3" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ))

(local.set $cmpresult (i8x16.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "store_data_4" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 0 1 2 3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "store_data_5" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 0 1 2 3 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "v128.load_offset_65521" ) ( drop ( v128.load offset=65521 ( i32.const 0 ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v128.load_offset_65521" (func $f 
                                 ))
  (func (export "run")
    (call $f )
    ))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1)
(func
  (drop (v128.load offset=-1 (i32.const 0)))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "v128.store_offset_65521" ) ( v128.store offset=65521 ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v128.store_offset_65521" (func $f 
                                 ))
  (func (export "run")
    (call $f )
    ))

`)), {'':ins.exports});
var thrown = false;
try { run.exports.run() } catch (e) { thrown = true; }
if (!thrown) throw 'Error: expected exception';
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1)
(func
  (v128.store offset=-1 (i32.const 0) (v128.const i32x4 0 0 0 0))
))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1)
(func (drop (v128.load offset=4294967296 (i32.const 0)))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1)
(func (v128.store offset=4294967296 (i32.const 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)

