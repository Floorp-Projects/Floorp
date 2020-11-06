// |jit-test| skip-if: !wasmIsSupported()

// Cranelift currently only supported on non-Windows ARM64.

var conf = getBuildConfiguration();
if (!conf.arm64 || conf.windows)
    assertEq(wasmCompileMode().indexOf("cranelift"), -1);
