// |jit-test| skip-if: !wasmGcEnabled()

// For the time being, we do not want to expose struct types outside of the
// module where they are defined, and so there are restrictions on how functions
// and global variables that traffic in struct types can be used.
//
// At the same time, intra-module uses of functions and globals that use struct
// types must be allowed to the greatest extent possible.
//
// Terminology: A function that takes a Ref parameter or returns a Ref result is
// "exposed for Ref", as is a global of Ref type.  EqRef is OK though, in all
// cases.
//
// To keep it simple we have the following restrictions that can all be checked
// statically.
//
//  - Exported and imported functions cannot be exposed for Ref.
//
//  - If the module has an exported or imported table then no function stored in
//    that table by the module (by means of an element segment) can be exposed
//    for Ref.
//
//  - If the module has an exported or imported table then no call_indirect via
//    that table may reference a type that is exposed for Ref.
//
//  - An exported or imported global cannot be exposed for Ref.
//
// Conversely,
//
//  - If a module has a private table then it can contain private functions that
//    are exposed for Ref and it is possible to call those functions via that
//    table.
//
//  - If a module has a private global then it can be exposed for ref.
//
// Note that
//
//  - code generators can work around the restrictions by instead using
//    functions and globals that use eqref, and by using downcasts to check
//    that the types are indeed correct.  (Though the meaning of downcast will
//    change as the GC feature evolves.)
//
//  - we could probably make the restrictions slightly softer but there's really
//    no advantage to doing so.

function wasmCompile(text) {
    return new WebAssembly.Module(wasmTextToBinary(text));
}

// Exported function can't take ref type parameter, but eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $x i32)))
      (func (export "f") (param (ref null $box)) (unreachable)))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (func (export "f") (param eqref) (unreachable)))`),
         "object");

// Exported function can't return ref result, but eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $x i32)))
      (func (export "f") (result (ref null $box)) (ref.null $box)))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (func (export "f") (result eqref) (ref.null eq)))`),
         "object");

// Imported function can't take ref parameter, but eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $x i32)))
      (import "m" "f" (func (param (ref null $box)))))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (import "m" "f" (func (param eqref))))`),
         "object");

// Imported function can't return ref type, but eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $x i32)))
      (import "m" "f" (func (param i32) (result (ref null $box)))))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (import "m" "f" (func (param i32) (result eqref))))`),
         "object");

// Imported global can't be of Ref type (irrespective of mutability), though eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (import "m" "g" (global (mut (ref null $box)))))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (import "m" "g" (global (ref null $box))))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (import "m" "g" (global (mut eqref))))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (import "m" "g" (global eqref)))`),
         "object");

// Exported global can't be of Ref type (irrespective of mutability), though eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (global $boxg (export "box") (mut (ref null $box)) (ref.null $box)))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (global $boxg (export "box") (ref null $box) (ref.null $box)))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (global $boxg (export "box") (mut eqref) (ref.null eq)))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (global $boxg (export "box") eqref (ref.null eq)))`),
         "object");

// Exported table cannot reference functions that are exposed for Ref, but eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (table (export "tbl") 1 funcref)
      (elem (i32.const 0) $f1)
      (func $f1 (param (ref null $box)) (unreachable)))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (table (export "tbl") 1 funcref)
      (elem (i32.const 0) $f1)
      (func $f1 (result (ref null $box)) (ref.null $box)))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (table (export "tbl") 1 funcref)
      (elem (i32.const 0) $f1)
      (func $f1 (param eqref) (unreachable)))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (table (export "tbl") 1 funcref)
      (elem (i32.const 0) $f1)
      (func $f1 (result eqref) (ref.null eq)))`),
         "object");

// Imported table cannot reference functions that are exposed for Ref, though eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (import "m" "tbl" (table 1 funcref))
      (elem (i32.const 0) $f1)
      (func $f1 (param (ref null $box)) (unreachable)))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (import "m" "tbl" (table 1 funcref))
      (elem (i32.const 0) $f1)
      (func $f1 (result (ref null $box)) (ref.null $box)))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (import "m" "tbl" (table 1 funcref))
      (elem (i32.const 0) $f1)
      (func $f1 (param eqref) (unreachable)))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (import "m" "tbl" (table 1 funcref))
      (elem (i32.const 0) $f1)
      (func $f1 (result eqref) (ref.null eq)))`),
         "object");

// Can't call via exported table with type that is exposed for Ref, though eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (type $fn (func (param (ref null $box))))
      (table (export "tbl") 1 funcref)
      (func (param i32)
       (call_indirect (type $fn) (ref.null $box) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (type $fn (func (result (ref null $box))))
      (table (export "tbl") 1 funcref)
      (func (param i32) (result (ref null $box))
       (call_indirect (type $fn) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (type $fn (func (param eqref)))
      (table (export "tbl") 1 funcref)
      (func (param i32)
       (call_indirect (type $fn) (ref.null eq) (local.get 0))))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (type $fn (func (result eqref)))
      (table (export "tbl") 1 funcref)
      (func (param i32) (result eqref)
       (call_indirect (type $fn) (local.get 0))))`),
         "object");

// Can't call via imported table with type that is exposed for Ref, though eqref is OK.

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (type $fn (func (param (ref null $box))))
      (import "m" "tbl" (table 1 funcref))
      (func (param i32)
       (call_indirect (type $fn) (ref.null $box) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertErrorMessage(() => wasmCompile(
    `(module
      (type $box (struct (field $val i32)))
      (type $fn (func (result (ref null $box))))
      (import "m" "tbl" (table 1 funcref))
      (func (param i32) (result (ref null $box))
       (call_indirect (type $fn) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /cannot expose indexed reference type/);

assertEq(typeof wasmCompile(
    `(module
      (type $fn (func (param eqref)))
      (import "m" "tbl" (table 1 funcref))
      (func (param i32)
       (call_indirect (type $fn) (ref.null eq) (local.get 0))))`),
         "object");

assertEq(typeof wasmCompile(
    `(module
      (type $fn (func (result eqref)))
      (import "m" "tbl" (table 1 funcref))
      (func (param i32) (result eqref)
       (call_indirect (type $fn) (local.get 0))))`),
         "object");

// We can call via a private table with a type that is exposed for Ref.

{
    let m = wasmCompile(
        `(module
          (type $box (struct (field $val i32)))
          (type $fn (func (param (ref null $box)) (result i32)))
          (table 1 funcref)
          (elem (i32.const 0) $f1)
          (func $f1 (param (ref null $box)) (result i32) (i32.const 37))
          (func (export "f") (param i32) (result i32)
           (call_indirect (type $fn) (ref.null $box) (local.get 0))))`);
    let i = new WebAssembly.Instance(m).exports;
    assertEq(i.f(0), 37);
}
