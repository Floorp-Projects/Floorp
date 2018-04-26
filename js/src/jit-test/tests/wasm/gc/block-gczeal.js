wasmEvalText(`(module
    (import "global" "func" (result i32))
    (func (export "func_0") (result i32)
     call 0 ;; calls the import, which is func #0
    )
)`, { global: {
    func() {
        gczeal(7,6);
        gczeal();
    }
} }).exports.func_0();
