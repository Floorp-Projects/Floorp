// |jit-test| exitstatus: 6;

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.
if (!wasmIsSupported())
    quit(6);

load(libdir + "asm.js");

var code = `
    var out = ffi.out;
    function f() {
        out();
    }
    return f;
`;

var ffi = {};
ffi.out = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func (export "f") (loop $top (br $top))))'))).exports.f;

timeout(1);
asmLink(asmCompile('glob', 'ffi', USE_ASM + code), this, ffi)();
assertEq(true, false);
