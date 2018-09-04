if (!wasmGcEnabled())
    quit(0);

// Basic private-to-module functionality.  At the moment all we have is null
// pointers, not very exciting.

{
    let bin = wasmTextToBinary(
        `(module

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

// We can't import a global of a reference type because we don't have a good
// notion of structural type compatibility yet.
{
    let bin = wasmTextToBinary(
        `(module
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
          (type $box (struct (field $val i32)))
          (global $boxg (export "box") (mut (ref $box)) (ref.null (ref $box))))`);

    assertErrorMessage(() => new WebAssembly.Module(bin), WebAssembly.CompileError,
                       /cannot expose reference type/);
}
