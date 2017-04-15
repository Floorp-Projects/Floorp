// |jit-test| exitstatus: 6;

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.
if (!wasmIsSupported())
    quit(6);

var g = newGlobal();
g.parent = this;
g.eval("Debugger(parent).onEnterFrame = function() {};");
timeout(0.01);
var code = wasmTextToBinary(`(module
    (func (export "f1")
        (loop $top br $top)
    )
)`);
new WebAssembly.Instance(new WebAssembly.Module(code)).exports.f1();
