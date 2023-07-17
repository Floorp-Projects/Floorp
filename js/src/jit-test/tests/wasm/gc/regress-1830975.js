// |jit-test| skip-if: !wasmGcEnabled()

const { test } = wasmEvalText(`
  (module
    (type $f (func (result i32)))
    (func (export "test") (type $f)
      ref.null $f
      ref.test (ref null $f)
    )
  )
`).exports;
assertEq(test(), 1);
