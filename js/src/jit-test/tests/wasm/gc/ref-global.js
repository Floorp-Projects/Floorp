// |jit-test| skip-if: !wasmReftypesEnabled() || !wasmGcEnabled() || wasmCompileMode() != 'baseline'

// Basic private-to-module functionality.  At the moment all we have is null
// pointers, not very exciting.

{
    let bin = wasmTextToBinary(
        `(module
          (type $point (struct
                        (field $x f64)
                        (field $y f64)))

          (global $g1 (mut (ref opt $point)) (ref.null))
          (global $g2 (mut (ref opt $point)) (ref.null))
          (global $g3 (ref opt $point) (ref.null))

          ;; Restriction: cannot expose Refs outside the module, not even
          ;; as a return value.  See ref-restrict.js.

          (func (export "get") (result anyref)
           (global.get $g1))

          (func (export "copy")
           (global.set $g2 (global.get $g1)))

          (func (export "clear")
           (global.set $g1 (global.get $g3))
           (global.set $g2 (ref.null))))`);

    let mod = new WebAssembly.Module(bin);
    let ins = new WebAssembly.Instance(mod).exports;

    assertEq(ins.get(), null);
    ins.copy();                 // Should not crash
    ins.clear();                // Should not crash
}

// Global with struct type

{
    let bin = wasmTextToBinary(
        `(module
          (type $point (struct
                        (field $x f64)
                        (field $y f64)))

          (global $glob (mut (ref opt $point)) (ref.null))

          (func (export "init")
           (global.set $glob (struct.new $point (f64.const 0.5) (f64.const 2.75))))

          (func (export "change")
           (global.set $glob (struct.new $point (f64.const 3.5) (f64.const 37.25))))

          (func (export "clear")
           (global.set $glob (ref.null)))

          (func (export "x") (result f64)
           (struct.get $point 0 (global.get $glob)))

          (func (export "y") (result f64)
           (struct.get $point 1 (global.get $glob))))`);

    let mod = new WebAssembly.Module(bin);
    let ins = new WebAssembly.Instance(mod).exports;

    assertErrorMessage(() => ins.x(), WebAssembly.RuntimeError, /dereferencing null pointer/);

    ins.init();
    assertEq(ins.x(), 0.5);
    assertEq(ins.y(), 2.75);

    ins.change();
    assertEq(ins.x(), 3.5);
    assertEq(ins.y(), 37.25);

    ins.clear();
    assertErrorMessage(() => ins.x(), WebAssembly.RuntimeError, /dereferencing null pointer/);
}

// Global value of type anyref for initializer from a WebAssembly.Global,
// just check that it works.
{
    let bin = wasmTextToBinary(
        `(module
          (import "" "g" (global $g anyref))
          (global $glob anyref (global.get $g))
          (func (export "get") (result anyref)
           (global.get $glob)))`);

    let mod = new WebAssembly.Module(bin);
    let obj = {zappa:37};
    let g = new WebAssembly.Global({value: "anyref"}, obj);
    let ins = new WebAssembly.Instance(mod, {"":{g}}).exports;
    assertEq(ins.get(), obj);
}

// We can't import a global of a reference type because we don't have a good
// notion of structural type compatibility yet.
{
    let bin = wasmTextToBinary(
        `(module
          (type $box (struct (field $val i32)))
          (import "m" "g" (global (mut (ref opt $box)))))`);

    assertErrorMessage(() => new WebAssembly.Module(bin), WebAssembly.CompileError,
                       /cannot expose indexed reference type/);
}

// We can't export a global of a reference type because we can't later import
// it.  (Once we can export it, the value setter must also perform the necessary
// subtype check, which implies we have some notion of exporting types, and we
// don't have that yet.)
{
    let bin = wasmTextToBinary(
        `(module
          (type $box (struct (field $val i32)))
          (global $boxg (export "box") (mut (ref opt $box)) (ref.null)))`);

    assertErrorMessage(() => new WebAssembly.Module(bin), WebAssembly.CompileError,
                       /cannot expose indexed reference type/);
}
