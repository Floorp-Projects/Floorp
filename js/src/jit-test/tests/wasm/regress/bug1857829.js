// |jit-test| --wasm-gc; skip-if: !wasmGcEnabled()

// Tests if i31ref global value is normalized.
var ins = wasmEvalText(`(module
    (global $i (ref i31) (ref.i31 (i32.const -1)))
    (func (export "f") (result i32)
       (ref.eq (ref.i31 (i32.const -1)) (global.get $i))
    )
 )`);
assertEq(ins.exports.f(), 1);

// OP test
var ins = wasmEvalText(`(module
    (import "env" "v" (func $g (result (ref eq))))
    (func (export "f")
       (local $v (ref eq))
       (local.set $v (call $g))
       (if (i32.eq (i31.get_s (ref.cast (ref i31) (local.get $v))) (i32.const -1))
          (then
            (if (i32.eqz (ref.eq (local.get $v) (ref.i31 (i32.const -1))))
               (then (unreachable))))))
)`, {env:{v:()=>-1}});
ins.exports.f();
