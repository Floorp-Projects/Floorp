// |jit-test| skip-if: !wasmFunctionReferencesEnabled()

// br_on_non_null from constant
wasmValidateText(`(module
  (func
    block (result (ref extern))
      ref.null extern
      br_on_non_null 0
      return
    end
    drop
  )
)`);

// br_on_non_null from parameter
wasmValidateText(`(module
  (func (param externref) (result (ref extern))
    local.get 0
    br_on_non_null 0
    unreachable
  )
)`);

// br_on_null with multiple results
wasmValidateText(`(module
  (func (param (ref null extern) (ref extern)) (result i32 i32 i32 (ref extern))
    i32.const 0
    i32.const 1
    i32.const 2
    local.get 0
    br_on_non_null 0
    local.get 1
  )
)`);

// no block type
wasmFailValidateText(`(module
  (func
    block
      ref.null extern
      br_on_non_null 0
    end
  )
)`, /type mismatch: target block type expected to be \[_, ref\]/);

// in dead code
wasmValidateText(`(module
  (type $t (func))
  (func (result funcref)
    ref.null $t
    return
    br_on_non_null 0
  )
)`);

wasmFailValidateText(`(module
  (func
    return
    br_on_non_null 0
  )
)`, /type mismatch: target block type expected to be \[_, ref\]/);

// Test the branch takes the correct path and results are passed correctly
let {ifNull} = wasmEvalText(`(module
  (func (export "ifNull") (param externref externref) (result externref)
    local.get 0
    br_on_non_null 0
    local.get 1
  )
)`).exports;

const DefaultTestVal = "default!test";
assertEq(ifNull(null, DefaultTestVal), DefaultTestVal);
for (let val of WasmNonNullExternrefValues) {
  assertEq(ifNull(val, DefaultTestVal), val);
}
