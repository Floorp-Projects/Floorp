// |jit-test| --wasm-tail-calls; --wasm-gc; skip-if: !wasmGcEnabled() || !wasmTailCallsEnabled()

// Tests if instance registers were restored properly when call_ref is used
// with tail calls.
var t = wasmEvalText(`(module
    (type $t1 (func))
    (func $f0 (param funcref i32 i32 i32 i32 i32 i32 i32 i32 i32)
        local.get 0
        ref.cast (ref $t1)
        return_call_ref $t1
    )
    (func $f1 (param i32))
    (elem declare func $f)
    (func $f (param funcref)
        (local i32 i32 i32 i32)
        local.get 0
        i32.const 1
        i32.const 1
        i32.const 1
        i32.const 1
        i32.const 1
        i32.const 1
        i32.const 1
        i32.const 1
        i32.const 1
        return_call $f0
    )
    (func (export "f") (result funcref)
        ref.func $f
    )
)`);

var t2 = wasmEvalText(`(module
    (import "" "f" (func $fi (result funcref)))
    (type $t1 (func (param funcref)))
    (elem declare func $f2)
    (func $f2)
    (func (export "test")
        ref.func $f2
        call $fi
        ref.cast (ref $t1)
        call_ref $t1
    )
)`, {"": {f:t.exports.f},});

t2.exports.test();
