// Exception tests that use reference types.

load(libdir + "eqArrayHelper.js");

assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param externref))
       (func (export "f") (result externref)
         try (result externref)
           ref.null extern
           throw $exn
         catch $exn
         end))`
  ).exports.f(),
  null
);

{
  let f = wasmEvalText(
    `(module
       (tag $exn (param funcref))
       (func $f (export "f") (result funcref)
         try (result funcref)
           ref.func $f
           throw $exn
         catch $exn
         end))`
  ).exports.f;
  assertEq(f(), f);
}

{
  let f = wasmEvalText(
    `(module
       (tag $exn (param externref))
       (func (export "f") (param externref) (result externref)
         try (result externref)
           local.get 0
           throw $exn
         catch $exn
         end))`
  ).exports.f;

  for (v of WasmExternrefValues) {
    assertEq(f(v), v);
  }
}

assertEqArray(
  wasmEvalText(
    `(module
       (tag $exn (param externref externref))
       (func (export "f") (param externref externref)
             (result externref externref)
         try (result externref externref)
           local.get 0
           local.get 1
           throw $exn
         catch $exn
         end))`
  ).exports.f("foo", "bar"),
  ["foo", "bar"]
);

assertEqArray(
  wasmEvalText(
    `(module
       (tag $exn (param i32 i32 externref f32))
       (func (export "f") (param externref)
             (result i32 i32 externref f32)
         try (result i32 i32 externref f32)
           i32.const 0
           i32.const 1
           local.get 0
           f32.const 2.0
           throw $exn
         catch $exn
         end))`
  ).exports.f("foo"),
  [0, 1, "foo", 2.0]
);

assertEqArray(
  wasmEvalText(
    `(module
       (tag $exn (param i32 i32 externref f32 externref f64))
       (func (export "f") (param externref externref)
             (result i32 i32 externref f32 externref f64)
         try (result i32 i32 externref f32 externref f64)
           i32.const 0
           i32.const 1
           local.get 0
           f32.const 2.0
           local.get 1
           f64.const 3.0
           throw $exn
         catch $exn
         end))`
  ).exports.f("foo", "bar"),
  [0, 1, "foo", 2.0, "bar", 3.0]
);

// Test to ensure that reference typed values are tracked correctly by
// the GC within exception objects.
{
  gczeal(2, 1); // Collect on every allocation.

  var thrower;
  let exports = wasmEvalText(
    `(module
       (tag $exn (param externref))
       (import "m" "f" (func $f (result externref)))
       (func (export "thrower") (param externref)
         (local.get 0)
         (throw $exn))
       (func (export "catcher") (result externref)
         try (result externref)
           (call $f)
         catch $exn
         end))`,
    {
      m: {
        f: () => {
          // The purpose of this intermediate try-catch in JS is to force
          // some allocation to occur after Wasm's `throw`, triggering GC, and
          // then rethrow back to Wasm to observe any errors.
          try {
            thrower("foo");
          } catch (e) {
            let v = { x: 5 };
            throw e;
          }
        },
      },
    }
  ).exports;

  thrower = exports.thrower;
  assertEq(exports.catcher(), "foo");
}
