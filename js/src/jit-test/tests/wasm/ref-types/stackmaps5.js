// Test for Bug 1739683. Tests that stackmaps are properly associated with
// an Ion call to a builtin Wasm instance method.

gczeal(2, 1); // Collect on every allocation.

let exports = wasmEvalText(
  `(module
     (table $tab (export "tab") 5 externref)
     (elem declare funcref (ref.func $g))
     (func $g)
     (func $f (export "f") (param externref) (result)
       (ref.func $g) ;; force a collection via allocation in instance call
       (ref.is_null)
       (if
         (then)
         (else (i32.const 0) (local.get 0) (table.set $tab)))
       )
     )`,
  {}
).exports;
exports.f("foo");
