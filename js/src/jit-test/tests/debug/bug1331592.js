// |jit-test| test-also=--wasm-compiler=optimizing; error: TestComplete

if (!wasmDebuggingEnabled())
     throw "TestComplete";

var module = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import "global" "func" (func (result i32)))
        (func (export "func_0") (result i32)
         call 0 ;; calls the import, which is func #0
        )
    )
`));

var dbg;
(function (global) {
    var dbgGlobal = newGlobal({newCompartment: true});
    dbg = new dbgGlobal.Debugger();
    dbg.addDebuggee(global);
})(this);

var instance = new WebAssembly.Instance(module, { global: { func: () => {
    var frame = dbg.getNewestFrame().older;
    frame.eval("some_error");
}}});
instance.exports.func_0();

throw "TestComplete";
