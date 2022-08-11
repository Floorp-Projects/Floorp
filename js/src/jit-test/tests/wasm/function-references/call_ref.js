// |jit-test| skip-if: !wasmFunctionReferencesEnabled()

let { plusOne } = wasmEvalText(`(module
  (; forward declaration so that ref.func works ;)
  (elem declare $plusOneRef)

  (func $plusOneRef (param i32) (result i32)
    (i32.add
      local.get 0
      i32.const 1)
  )

  (func (export "plusOne") (param i32) (result i32)
    local.get 0
    ref.func $plusOneRef
    call_ref
  )
)`).exports;

assertEq(plusOne(3), 4);

// pass non-funcref type
wasmFailValidateText(`(module
  (func (param $a i32)
    local.get $a
    call_ref
  )
)`, /type mismatch: expression has type i32 but expected a reference type/);

wasmFailValidateText(`(module
  (func (param $a (ref extern))
    local.get $a
    call_ref
  )
)`, /type mismatch: reference expected to be a subtype of funcref/);

// pass (non-subtype of) funcref
wasmFailValidateText(`(module
  (func (param funcref)
    local.get 0
    call_ref
  )
)`, /type mismatch: reference expected to be a subtype of funcref/);  
  
// signature mismatch
wasmFailValidateText(`(module
  (elem declare $plusOneRef)
  (func $plusOneRef (param f32) (result f32)
    (f32.add
      local.get 0
      f32.const 1.0)
  )

  (func (export "plusOne") (param i32) (result i32)
    local.get 0
    ref.func $plusOneRef
    call_ref
  )
)`, /type mismatch/);

// TODO fix call_ref is not allowed in dead code
wasmFailValidateText(`(module
  (func (param $a i32)
    unreachable
    call_ref
  )
)`, /gc instruction temporarily not allowed in dead code/);

// Cross-instance calls
let { loadInt } = wasmEvalText(`(module
  (memory 1 1)
  (data (i32.const 0) "\\04\\00\\00\\00")
  (func (export "loadInt") (result i32)
    i32.const 0
    i32.load offset=0
  )
)`).exports;

let { callLoadInt } = wasmEvalText(`(module
  (elem declare 0)
  (import "" "loadInt" (func (result i32)))
  (func (export "callLoadInt") (result i32)
    ref.func 0
    call_ref
  )
)`, {"": { loadInt, }}).exports;

assertEq(loadInt(), 4);
assertEq(callLoadInt(), 4);

// Null call.
assertErrorMessage(function() {
  let { nullCall } = wasmEvalText(`(module
    (type $t (func (param i32) (result i32)))
    (func (export "nullCall") (param i32) (result i32)
      local.get 0
      ref.null $t
      call_ref
    )
  )`).exports;
  nullCall(3);
}, WebAssembly.RuntimeError, /dereferencing null pointer/);
