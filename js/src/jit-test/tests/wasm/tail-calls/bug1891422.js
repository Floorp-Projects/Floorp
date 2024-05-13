// |jit-test| --more-compartments; skip-variant-if: --setpref=wasm_test_serialization=true, true; skip-variant-if: --wasm-compiler=ion, true

a = newGlobal({ newCompartment: true });
a.b = this;
a.eval(`Debugger(b).onExceptionUnwind = function () {};`);

var ins0 = wasmEvalText(`(module
  (func $fac-acc (export "e") (param i64 i64)
    unreachable
  )
)`);
var ins = wasmEvalText(`(module
  (import "" "e" (func $fac-acc (param i64 i64)))
  (type $tz (func (param i64)))
  (table $t 1 1 funcref)
  (func $f (export "fac") (param i64)
    local.get 0
    i32.const 0
    return_call_indirect $t (type $tz)
  )
  (elem $t (i32.const 0) $fac-acc)
)`, {"": {e: ins0.exports.e}});


assertErrorMessage(() => {
  ins.exports.fac(5n);
}, WebAssembly.RuntimeError, /indirect call signature mismatch/);
