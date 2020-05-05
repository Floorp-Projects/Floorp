var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "v128.store_i8x16" ) ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "v128.store_i16x8" ) ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const i16x8 0 1 2 3 4 5 6 7 ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "v128.store_i16x8_2" ) ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const i16x8 012_345 012_345 012_345 012_345 012_345 012_345 012_345 012_345 ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "v128.store_i16x8_3" ) ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const i16x8 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 0x0_1234 ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "v128.store_i32x4" ) ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 1 2 3 ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "v128.store_i32x4_2" ) ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const i32x4 0_123_456_789 0_123_456_789 0_123_456_789 0_123_456_789 ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "v128.store_i32x4_3" ) ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const i32x4 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 0x0_1234_5678 ) ) ( v128.load ( i32.const 0 ) ) ) ( func ( export "v128.store_f32x4" ) ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const f32x4 0 1 2 3 ) ) ( v128.load ( i32.const 0 ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v128.store_i8x16" (func $f (param ) (result v128)))
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
  (import "" "v128.store_i16x8" (func $f (param ) (result v128)))
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
  (import "" "v128.store_i16x8_2" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i16x8 12345 12345 12345 12345 12345 12345 12345 12345 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v128.store_i16x8_3" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i16x8 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 0x1234 ))

(local.set $cmpresult (i16x8.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v128.store_i32x4" (func $f (param ) (result v128)))
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
  (import "" "v128.store_i32x4_2" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 123456789 123456789 123456789 123456789 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v128.store_i32x4_3" (func $f (param ) (result v128)))
  (func (export "run") (result i32) 
(local $result v128)
(local $expected v128)
(local $cmpresult v128)

(local.set $result (call $f ))
(local.set $expected ( v128.const i32x4 0x12345678 0x12345678 0x12345678 0x12345678 ))

(local.set $cmpresult (i32x4.eq (local.get $result) (local.get $expected)))
(i8x16.all_true (local.get $cmpresult))))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "v128.store_f32x4" (func $f (param ) (result v128)))
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
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
( module ( memory 1 ) ( func ( export "as-block-value" ) ( block ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) ) ( func ( export "as-loop-value" ) ( loop ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) ) ( func ( export "as-br-value" ) ( block ( br 0 ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) ) ) ( func ( export "as-br_if-value" ) ( block ( br_if 0 ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ( i32.const 1 ) ) ) ) ( func ( export "as-br_if-value-cond" ) ( block ( br_if 0 ( i32.const 6 ) ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) ) ) ( func ( export "as-br_table-value" ) ( block ( br_table 0 ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ( i32.const 1 ) ) ) ) ( func ( export "as-return-value" ) ( return ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) ) ( func ( export "as-if-then" ) ( if ( i32.const 1 ) ( then ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) ) ) ( func ( export "as-if-else" ) ( if ( i32.const 0 ) ( then ) ( else ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) ) ) )
`)));
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-block-value" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-loop-value" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-br-value" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-br_if-value" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-br_if-value-cond" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-br_table-value" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-return-value" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-if-then" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var run = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`

(module
  (import "" "as-if-else" (func $f (param )))
  (func (export "run") (result i32)
    (call $f )
    (i32.const 1)))

`)), {'':ins.exports});
assertEq(run.exports.run(), 1)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1)
(func (v128.store8 (i32.const 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1)
(func (v128.store16 (i32.const 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
try { wasmTextToBinary(`
(module 
(memory 1)
(func (v128.store32 (i32.const 0) (v128.const i32x4 0 0 0 0))))
`) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof SyntaxError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 1 ) ( func ( v128.store ( f32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 1 ) ( func ( local v128 ) ( block ( br_if 0 ( v128.store ) ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 1 ) ( func ( result v128 ) ( v128.store ( i32.const 0 ) ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func $v128.store-1st-arg-empty ( v128.store ( v128.const i32x4 0 0 0 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func $v128.store-2nd-arg-empty ( v128.store ( i32.const 0 ) ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)
var thrown = false;
var saved;
var bin = wasmTextToBinary(`
( module ( memory 0 ) ( func $v128.store-arg-empty ( v128.store ) ) )
`);
assertEq(WebAssembly.validate(bin), false);
try { new WebAssembly.Module(bin) } catch (e) { thrown = true; saved = e; }
assertEq(thrown, true)
assertEq(saved instanceof WebAssembly.CompileError, true)

