if (!wasmGcEnabled())
    quit(0);

// Basic private-to-module functionality.  At the moment all we have is null
// pointers, not very exciting.

{
    let bin = wasmTextToBinary(
        `(module
          (gc_feature_opt_in 1)

          (type $point (struct
                        (field $x f64)
                        (field $y f64)))

          (global $g1 (mut (ref $point)) (ref.null (ref $point)))
          (global $g2 (mut (ref $point)) (ref.null (ref $point)))
          (global $g3 (ref $point) (ref.null (ref $point)))

          ;; Restriction: cannot expose Refs outside the module, not even
          ;; as a return value.  See ref-restrict.js.

          (func (export "get") (result anyref)
           (get_global $g1))

          (func (export "copy")
           (set_global $g2 (get_global $g1)))

          (func (export "clear")
           (set_global $g1 (get_global $g3))
           (set_global $g2 (ref.null (ref $point)))))`);

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
          (gc_feature_opt_in 1)

          (type $point (struct
                        (field $x f64)
                        (field $y f64)))

          (global $glob (mut (ref $point)) (ref.null (ref $point)))

          (func (export "init")
           (set_global $glob (struct.new $point (f64.const 0.5) (f64.const 2.75))))

          (func (export "change")
           (set_global $glob (struct.new $point (f64.const 3.5) (f64.const 37.25))))

          (func (export "clear")
           (set_global $glob (ref.null (ref $point))))

          (func (export "x") (result f64)
           (struct.get $point 0 (get_global $glob)))

          (func (export "y") (result f64)
           (struct.get $point 1 (get_global $glob))))`);

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
          (gc_feature_opt_in 1)
          (import $g "" "g" (global anyref))
          (global $glob anyref (get_global $g))
          (func (export "get") (result anyref)
           (get_global $glob)))`);

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
          (gc_feature_opt_in 1)
          (type $box (struct (field $val i32)))
          (import "m" "g" (global (mut (ref $box)))))`);

    assertErrorMessage(() => new WebAssembly.Module(bin), WebAssembly.CompileError,
                       /cannot expose reference type/);
}

// We can't export a global of a reference type because we can't later import
// it.  (Once we can export it, the value setter must also perform the necessary
// subtype check, which implies we have some notion of exporting types, and we
// don't have that yet.)
{
    let bin = wasmTextToBinary(
        `(module
          (gc_feature_opt_in 1)
          (type $box (struct (field $val i32)))
          (global $boxg (export "box") (mut (ref $box)) (ref.null (ref $box))))`);

    assertErrorMessage(() => new WebAssembly.Module(bin), WebAssembly.CompileError,
                       /cannot expose reference type/);
}
