// |jit-test| skip-if: !wasmIsSupported()
let binary = wasmTextToBinary(`
(module
    (import "" "visit" (func $visit (param externref) (result i32)))
    (func $wasmfunc
        (export "wasmfunc")
        (param $p1 externref)
        (param $p2 i32)
        (param $p3 externref)
        (param $p4 externref)
        (param $p5 externref)
        (param $p6 externref)
        (param $p7 externref)
        (param $p8 externref)
        (drop (call $visit (local.get $p1)))
    )
)`);
let mod = new WebAssembly.Module(binary);
let depth = 0;
function f() {
    if (depth++ < 25) {
        instance.exports.wasmfunc();
    }
};
let imports = {visit: f};
let instance = new WebAssembly.Instance(mod, {"": imports});
gczeal(2);
f();
assertEq(depth, 26);
