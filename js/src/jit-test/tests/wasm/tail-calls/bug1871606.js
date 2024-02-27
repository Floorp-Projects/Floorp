// |jit-test| --more-compartments; skip-variant-if: --setpref=wasm_test_serialization=true, true; skip-variant-if: --wasm-compiler=ion, true
dbg = newGlobal();
dbg.b = this;
dbg.eval("(" + function() {
  Debugger(b)
} + ")()");
var ins = wasmEvalText(`
   (table 1 funcref)
   (type $d (func (param f64)))
   (func $e
    (export "test")
     f64.const 0.0
     i32.const 0
     return_call_indirect (type $d)
   )
   (elem (i32.const 0) $e)
`);

assertErrorMessage(
  () => ins.exports.test(),
  WebAssembly.RuntimeError, /indirect call signature mismatch/
);
