// |jit-test| exitstatus: 6;

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.
if (!wasmIsSupported())
    quit(6);

var tbl = new WebAssembly.Table({initial:1, element:"anyfunc"});

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`(module
    (func $iloop
        loop $top
            br $top
        end
    )
    (import "imports" "tbl" (table 1 anyfunc))
    (elem (i32.const 0) $iloop)
)`)), {imports:{tbl}});

var outer = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`(module
    (import "imports" "tbl" (table 1 anyfunc))
    (type $v2v (func))
    (func (export "run")
        i32.const 0
        call_indirect $v2v
    )
)`)), {imports:{tbl}});

timeout(1, () => { tbl.set(0, null); gc() });
outer.exports.run();
assertEq(true, false);
