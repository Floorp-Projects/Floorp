// |jit-test| skip-if: !getBuildConfiguration("arm"); test-also=--arm-hwcap=armv7,vfp

// The command line options disable the idiv instruction and thus make the
// baseline compiler unavailable, so we must be prepared for a run-time error
// below in the baseline-only configuration.

// The flags above should be sufficient for there to be a WebAssembly object.
assertEq(typeof WebAssembly, "object");

try {
    var i = new WebAssembly.Instance(
        new WebAssembly.Module(
            wasmTextToBinary('(module (func (export "") (result i32) (i32.const 42)))')));
    assertEq(i.exports[""](), 42);
} catch (e) {
    if (String(e).match(/no WebAssembly compiler available/)) {
        switch (wasmCompileMode()) {
        case "none":
            // This is fine: the limited feature set combined with command line
            // compiler selection caused all compilers to be disabled.
            break;
        default:
            // This is not fine: if there's a compiler available then we should
            // not get an error.
            throw e;
        }
    } else {
        // Some other error, propagate it.
        throw e;
    }
}
