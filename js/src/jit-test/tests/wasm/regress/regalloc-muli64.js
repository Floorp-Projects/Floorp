// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

// Bug 1298808.
assertEq(wasmEvalText(`(module
    (func
        (result i32)
        (i32.wrap/i64
            (i64.mul
                ;; Conditions: rhs == lhs, rhs is not a constant.
                (i64.add (i64.const 1) (i64.const 10))
                (i64.add (i64.const 1) (i64.const 10))
            )
        )
    )
    (export "" 0)
)`).exports[""](), 121);
