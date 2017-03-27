// |jit-test| exitstatus: 6;

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.
if (!wasmIsSupported())
    quit(6);

// Single-step profiling currently only works in the ARM simulator
if (!getBuildConfiguration()["arm-simulator"])
    quit(6);

var code = wasmTextToBinary(`(module
    (func (export "iloop")
        (loop $top br $top)
    )
)`);

enableGeckoProfiling();
enableSingleStepProfiling();
var i = new WebAssembly.Instance(new WebAssembly.Module(code));
timeout(.1);
i.exports.iloop();
assertEq(true, false);
