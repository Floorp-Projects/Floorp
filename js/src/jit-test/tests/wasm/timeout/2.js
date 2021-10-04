// |jit-test| exitstatus: 6; skip-if: !wasmIsSupported()

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.

var tbl = new WebAssembly.Table({initial:1, element:"funcref"});

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`(module
    (import "imports" "tbl" (table 1 funcref))
    (func $iloop
        loop $top
            br $top
        end
    )
    (elem (i32.const 0) $iloop)
)`)), {imports:{tbl}});

var outer = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`(module
    (import "imports" "tbl" (table 1 funcref))
    (type $v2v (func))
    (func (export "run")
        i32.const 0
        call_indirect (type $v2v)
    )
)`)), {imports:{tbl}});

setJitCompilerOption('simulator.always-interrupt', 1);
timeout(1, () => { tbl.set(0, null); gc() });
outer.exports.run();
assertEq(true, false);
