// |jit-test| exitstatus: 6;

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.
if (!wasmIsSupported())
    quit(6);

newGlobal().Debugger().addDebuggee(this);

var t = new WebAssembly.Table({
    initial: 1,
    element: "anyfunc"
});

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
    (func $iloop loop $top br $top end)
    (import "imports" "t" (table1 anyfunc))
    (elem (i32.const0) $iloop))
`)), { imports: { t } });

outer = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
    (import "imports" "t" (table1 anyfunc))
    (type $v2v (func))
    (func (export "run")
        i32.const0
        call_indirect $v2v)
    )`)), { imports: { t } });

setJitCompilerOption('simulator.always-interrupt', 1);
timeout(1);
outer.exports.run();
