// |jit-test| skip-variant-if: --setpref=wasm_test_serialization=true, true; skip-if: !wasmGcEnabled()

gczeal(18)
function a(str, imports) {
  b = wasmTextToBinary(str)
  try {
    c = new WebAssembly.Module(b)
  } catch {}
  return new WebAssembly.Instance(c, imports)
}
gczeal(10)
base = a(`(module
  (global (export "rngState")
    (mut i32) i32.const 1
  )
  (global $ttl (mut i32) (i32.const 100000))

  (type $d (array (mut i32)))
  (type $e (array (mut (ref null $d))))

  (func $f 
    (param $arr (ref $d))
    (result (ref $d))

    (global.set $ttl (i32.sub (global.get $ttl) (i32.const 1)))
    (if (i32.eqz (global.get $ttl))
      (then
        unreachable
      )
    )

    local.get $arr
  )
  (func $createSecondaryArray (result (ref $d))
    (return_call $f
      (array.new $d (i32.const 0) (i32.const 1))
    )
  )
  (func $g (export "createPrimaryArrayLoop")
    (param i32) (param $arrarr (ref $e)) 
    (result (ref $e))

    (call $createSecondaryArray (local.get $arrarr))
    (return_call $g
      (i32.const 1)
      local.get $arrarr
    )
  )
)`)
t58 = `(module
  (type $d (array (mut i32)))
  (type $e (array (mut (ref null $d))))
  
  (import "" "rngState" (global $h (mut i32)))
  (import "" "createPrimaryArrayLoop" (func $g (param i32 (ref $e)) (result (ref $e))))

  (func $createPrimaryArray (result (ref $e))
    (return_call $g
      i32.const 0
      (array.new $e (ref.null $d) (i32.const 500))
    )
  )
  (func (export "churn") (result i32)
    (local $arrarr (ref $e))
    (local.set $arrarr (call $createPrimaryArray))

    (global.get $h)
  )
)`
j = a(t58, {"": base.exports})
fns = j.exports;

assertErrorMessage(() => {
  fns.churn();
}, WebAssembly.RuntimeError, /unreachable executed/);
