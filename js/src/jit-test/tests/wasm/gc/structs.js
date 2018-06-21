if (!wasmGcEnabled()) {
    assertErrorMessage(() => wasmEvalText(`(module (type $s (struct)))`),
                       WebAssembly.CompileError, /Structure types not enabled/);
    quit();
}

var bin = wasmTextToBinary(
    `(module

      (table 2 anyfunc)
      (elem (i32.const 0) $doit $doitagain)

      ;; Type array has a mix of types

      (type $f1 (func (param i32) (result i32)))

      (type $point (struct
                    (field $point_x i32)
                    (field $point_y i32)))

      (type $f2 (func (param f64) (result f64)))

      (type $int_node (struct
                       (field $intbox_val (mut i32))
                       (field $intbox_next (mut anyref))))

      ;; Test all the types.

      (type $omni (struct
                   (field $omni_i32 i32)
                   (field $omni_i32m (mut i32))
                   (field $omni_i64 i64)
                   (field $omni_i64m (mut i64))
                   (field $omni_f32 f32)
                   (field $omni_f32m (mut f32))
                   (field $omni_f64 f64)
                   (field $omni_f64m (mut f64))
                   (field $omni_anyref anyref)
                   (field $omni_anyrefm (mut anyref))))

      ;; Various ways to reference a type in the middle of the
      ;; type array, make sure we get the right one

      (func $x1 (import "m" "x1") (type $f1))
      (func $x2 (import "m" "x2") (type $f2))

      (func (export "hello") (param f64) (param i32) (result f64)
       (call_indirect $f2 (get_local 0) (get_local 1)))

      (func $doit (param f64) (result f64)
       (f64.sqrt (get_local 0)))

      (func $doitagain (param f64) (result f64)
       (f64.mul (get_local 0) (get_local 0)))

      (func (export "x1") (param i32) (result i32)
       (call $x1 (get_local 0)))

      (func (export "x2") (param f64) (result f64)
       (call $x2 (get_local 0)))
     )`)

var mod = new WebAssembly.Module(bin);
var ins = new WebAssembly.Instance(mod, {m:{x1(x){ return x*3 }, x2(x){ return Math.PI }}}).exports;

assertEq(ins.hello(4.0, 0), 2.0)
assertEq(ins.hello(4.0, 1), 16.0)

assertEq(ins.x1(12), 36)
assertEq(ins.x2(8), Math.PI)

// The field name is optional, so this should work.

wasmEvalText(`
(module
 (type $s (struct (field i32))))
`)

// Empty structs are OK.

wasmEvalText(`
(module
 (type $s (struct)))
`)

// Bogus type definition syntax.

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s))
`),
SyntaxError, /parsing wasm text/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (field $x i32)))
`),
SyntaxError, /bad type definition/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field $x i31))))
`),
SyntaxError, /parsing wasm text/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (fjeld $x i32))))
`),
SyntaxError, /parsing wasm text/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct abracadabra)))
`),
SyntaxError, /parsing wasm text/);

// Function should not reference struct type: syntactic test

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct))
 (type $f (func (param i32) (result i32)))
 (func (type 0) (param i32) (result i32) (unreachable)))
`),
WebAssembly.CompileError, /signature index references non-signature/);

// Function should not reference struct type: binary test

var bad = new Uint8Array([0x00, 0x61, 0x73, 0x6d,
                          0x01, 0x00, 0x00, 0x00,

                          0x01,                   // Type section
                          0x03,                   // Section size
                          0x01,                   // One type
                          0x50,                   // Struct
                          0x00,                   // Zero fields

                          0x03,                   // Function section
                          0x02,                   // Section size
                          0x01,                   // One function
                          0x00,                   // Type of function

                          0x0a,                   // Code section
                          0x05,                   // Section size
                          0x01,                   // One body
                          0x03,                   // Body size
                          0x00,                   // Zero locals
                          0x00,                   // UNREACHABLE
                          0x0b]);                 // END

assertErrorMessage(() => new WebAssembly.Module(bad),
                   WebAssembly.CompileError, /signature index references non-signature/);
