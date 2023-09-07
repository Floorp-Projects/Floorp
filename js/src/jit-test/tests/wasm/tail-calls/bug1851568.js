wasmFailValidateText(`(module
    (func (result i32 f64)
      i32.const 1
      f64.const 2.0
    )
    (func (export "f") (result f64)
      return_call 0
    )
)`, /type mismatch/);

wasmFailValidateText(`(module
    (func (result i32 f64)
      i32.const 1
      f64.const 2.0
    )
    (func (export "f") (result f32 i32 f64)
      f32.const 3.14
      return_call 0
    )
)`, /type mismatch/);
