// |jit-test| exitstatus: 6; skip-if: !wasmIsSupported()

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.

var code = wasmTextToBinary(`(module
    (func (export "iloop")
        (loop $top br $top)
    )
)`);

var i = new WebAssembly.Instance(new WebAssembly.Module(code));
timeout(1);
i.exports.iloop();
assertEq(true, false);
