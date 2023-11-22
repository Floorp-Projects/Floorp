// |jit-test| skip-if: !wasmGcEnabled()

// Casting an external value is an expected failure
let { refCast, refTest, brOnCast, brOnCastFail } = wasmEvalText(`
  (module
    (; give this struct a unique identity to avoid conflict with
       the struct type defined in wasm.js ;)
    (type (struct (field i64 i32 i64 i32)))

    (func (export "refTest") (param externref) (result i32)
      local.get 0
      any.convert_extern
      ref.test (ref 0)
    )
    (func (export "refCast") (param externref) (result i32)
      local.get 0
      any.convert_extern
      ref.cast (ref null 0)
      drop
      i32.const 0
    )
    (func (export "brOnCast") (param externref) (result i32)
      (block (result (ref 0))
        local.get 0
        any.convert_extern
        br_on_cast 0 anyref (ref 0)
        drop
        i32.const 0
        br 1
      )
      drop
      i32.const 1
    )
    (func (export "brOnCastFail") (param externref) (result i32)
      (block (result anyref)
        local.get 0
        any.convert_extern
        br_on_cast_fail 0 anyref (ref 0)
        drop
        i32.const 1
        br 1
      )
      drop
      i32.const 0
    )
  )
`).exports;

for (let v of WasmNonNullExternrefValues) {
  assertErrorMessage(() => refCast(v), WebAssembly.RuntimeError, /bad cast/);
  assertEq(refTest(v), 0);
  assertEq(brOnCast(v), 0);
  assertEq(brOnCastFail(v), 0);
}
