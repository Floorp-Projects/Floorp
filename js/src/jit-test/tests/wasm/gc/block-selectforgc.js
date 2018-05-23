let { exports } = wasmEvalText(`
    (module
        (import "global" "func" (result i32))
        (func (export "func_0") (result i32)
         call 0 ;; calls the import, which is func #0
        )
    )
`, {
    global: {
        func() {
            selectforgc();
        }
    }
});

try {
    exports.func_0();
} catch(e) {
    // If there's an error, it must be that the addMarkObservers API is
    // temporarily disabled because of wasm gc.
    assertEq(/temporarily unavailable/.test(e.message), true);
}
