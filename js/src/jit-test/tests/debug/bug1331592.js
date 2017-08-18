// |jit-test| test-also-no-wasm-baseline; error: TestComplete

if (!wasmDebuggingIsSupported())
     throw "TestComplete";

var module = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import "global" "func" (result i32))
        (func (export "func_0") (result i32)
         call 0 ;; calls the import, which is func #0
        )
    )
`));

var dbg;
(function (global) {
    var dbgGlobal = newGlobal();
    dbg = new dbgGlobal.Debugger();
    dbg.addDebuggee(global);
})(this);

var instance = new WebAssembly.Instance(module, { global: { func: () => {
    var frame = dbg.getNewestFrame().older;
    frame.eval("some_error");
}}});
instance.exports.func_0();

throw "TestComplete";
