wasmEvalText(`
    (module
        (import "global" "func" (result i32))
        (func (export "func_0") (result i32)
         call 0
        )
    )
`, {
    global: {
        func() {
            verifyprebarriers();
        }
    }
}).exports.func_0();
