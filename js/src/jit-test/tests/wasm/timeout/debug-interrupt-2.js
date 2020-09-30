// |jit-test| exitstatus: 6; skip-if: !wasmDebuggingEnabled()

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.

// Note: this test triggers an interrupt and then iloops in Wasm code. If the
// interrupt fires before the Wasm code is compiled, the test relies on the
// JS interrupt check in onEnterFrame to catch it. Warp/Ion code however can
// elide the combined interrupt/overrecursion check in simple leaf functions.
// Long story short, set the Warp threshold to a non-zero value to prevent
// intermittent timeouts with --ion-eager.
setJitCompilerOption("ion.warmup.trigger", 30);

var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("Debugger(parent).onEnterFrame = function() {};");
timeout(0.01);
var code = wasmTextToBinary(`(module
    (func $f2
        loop $top
            br $top
        end
    )
    (func (export "f1")
        call $f2
    )
)`);
new WebAssembly.Instance(new WebAssembly.Module(code)).exports.f1();
