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

// can have non-defaultable local, but not use/get if unset.
wasmValidateText(`(module
  (func (local (ref extern)))
)`);
wasmFailValidateText(`(module
  (func (local (ref extern))
    local.get 0
    drop
  )
)`, /local\.get read from unset local/);
wasmFailValidateText(`(module
  (func
    (local (ref extern))
    unreachable
    block
      local.get 0
      drop
    end
  )
)`, /local\.get read from unset local/);
wasmFailValidateText(`(module
  (func (param funcref) (result funcref) (local (ref func))
    block
      local.get 0
      ref.as_non_null
      local.set 1
    end
    local.get 1
  )
)`, /local\.get read from unset local/);
wasmValidateText(`(module
  (func (param $r (ref extern))
    (local $var (ref extern))
    local.get $r
    ref.as_non_null
    local.set $var
    block block block
    local.get $var
    drop
    end end end
  )
  (func
    (param (ref null func) (ref null func) (ref func))
    (result funcref)
    (local (ref func) i32 (ref func) (ref null func))
    local.get 0
    ref.as_non_null
    local.tee 3
    block
      local.get 6
      ref.as_non_null
      local.set 5 
    end
    local.get 2
    drop
    local.tee 5
  )  
)`);
wasmFailValidateText(`(module
  (elem declare func 0)
  (func
    (local (ref func))
    i32.const 0
    if
      ref.func 0
      local.set 0
    else
      local.get 0
      drop
    end
  )
)`, /local\.get read from unset local/);
wasmValidateText(`(module
  (elem declare func 0)
  (func (result funcref)
    (local (ref func) (ref func))
    i32.const 0
    if (result funcref)
      ref.func 0
      local.set 0
      local.get 0
    else
      ref.func 0
      local.tee 1
      local.get 1
      drop
    end
  )
)`);

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

{
  // can have non-nullable globals
  wasmEvalText(`(module
    (func $f)
    (elem declare func $f)
    (global (ref func) ref.func $f)
  )`);
}

// cannot have non-nullable tables without initializer
wasmFailValidateText(`(module
  (table (ref extern) (elem))
)`, /table with non-nullable references requires initializer/);

// Testing internal wasmLosslessInvoke to pass non-nullable as params and arguments.
let {t} = wasmEvalText(`(module
  (func (export "t") (param (ref extern)) (result (ref extern))
    (local (ref extern))
    (local.set 1 (local.get 0))
    (local.get 1)
  )
)`).exports;
const ret = wasmLosslessInvoke(t, {test: 1});
assertEq(ret.value.test, 1);
