// |jit-test| --more-compartments; skip-variant-if: --wasm-test-serialization, true; skip-variant-if: --wasm-compiler=ion, true; skip-if: !wasmGcEnabled()

var dbg = newGlobal()
dbg.parent = this
dbg.eval(`
      Debugger(parent).onEnterFrame = function() {}
`)

var wasm = `(module
    (type $t1 (array (mut i32))) 
    (type $t2 (array (mut (ref null $t1))))
    (import "" "c" 
      (func $c
        (param i32 (ref $t2)) 
        (result (ref $t2))))
    (func $f (result (ref $t2))
      (return_call $c
        (i32.const 0)
        (array.new $t2 (ref.null $t1) (i32.const 1500)))
    )
   (func (export "test")
     (drop (call $f))
   )
)`;

var ins = wasmEvalText(wasm, {"": { c() {},}});
assertErrorMessage(
  () => ins.exports.test(),
  TypeError, /bad type/
);
