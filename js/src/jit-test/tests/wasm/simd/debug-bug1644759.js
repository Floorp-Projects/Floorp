// |jit-test| skip-if: !wasmDebuggingEnabled() || !wasmSimdEnabled()

var g7 = newGlobal({newCompartment: true});
g7.parent = this;
g7.eval(`
    Debugger(parent).onEnterFrame = function(frame) { };
`);
var ins = wasmEvalText(`
    (memory (export "mem") 1 1)
    (func (export "run")
      (param $k i32)
      (v128.store (i32.const 0) (call $f (local.get $k)))
    )
    (func $f
      (param $k i32)
      (result v128)
      (v128.const i32x4 5 6 7 8)
    )
`);
ins.exports.run(0);
