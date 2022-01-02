// |jit-test| skip-if: !wasmFunctionReferencesEnabled()

// non-null values are subtype of null values
wasmValidateText(`(module
  (func (param $a (ref extern))
    local.get $a
    (block (param (ref null extern))
      drop
    )
  )
)`);

// null values are not subtype of non-null values
wasmFailValidateText(`(module
  (func (param $a (ref null extern))
    local.get $a
    (block (param (ref extern))
      drop
    )
  )
)`, /expression has type externref but expected \(ref extern\)/);

// cannot have non-defaultable local
wasmFailValidateText(`(module
  (func (local (ref extern)))
)`, /cannot have a non-defaultable local/);

// exported funcs can't take null in non-nullable params
let {a} = wasmEvalText(`(module
  (func (export "a") (param (ref extern)))
)`).exports;
assertErrorMessage(() => a(null), TypeError, /cannot pass null to non-nullable/);
for (let val of WasmNonNullExternrefValues) {
  a(val);
}

// imported funcs can't return null in non-nullable results
function returnNull() {
  return null;
}
function returnMultiNullReg() {
  return [null, null];
}
function returnMultiNullStack() {
  return [1, 2, 3, 4, 5, 6, 7, 8, null];
}
let {runNull, runMultiNullReg, runMultiNullStack} = wasmEvalText(`(module
  (func $returnNull (import "" "returnNull") (result (ref extern)))
  (func $returnMultiNullReg (import "" "returnMultiNullReg") (result (ref extern) (ref extern)))
  (func $returnMultiNullStack (import "" "returnMultiNullStack") (result (ref extern) (ref extern) (ref extern) (ref extern) (ref extern) (ref extern) (ref extern) (ref extern) (ref extern)))
  (func (export "runNull")
    call $returnNull
    unreachable
  )
  (func (export "runMultiNullReg")
    call $returnMultiNullReg
    unreachable
  )
  (func (export "runMultiNullStack")
    call $returnMultiNullStack
    unreachable
  )
)`, { "": { returnNull, returnMultiNullReg, returnMultiNullStack } }).exports;
assertErrorMessage(() => runNull(), TypeError, /cannot pass null to non-nullable/);
assertErrorMessage(() => runMultiNullReg(), TypeError, /cannot pass null to non-nullable/);
assertErrorMessage(() => runMultiNullStack(), TypeError, /cannot pass null to non-nullable/);

// cannot have non-nullable globals
wasmFailValidateText(`(module
  (global $a (import "" "") (ref extern))
  (global (ref extern) global.get $a)
)`, /non-nullable references not supported in globals/);

// cannot have non-nullable tables
wasmFailValidateText(`(module
  (table (ref extern) (elem))
)`, /non-nullable references not supported in tables/);
