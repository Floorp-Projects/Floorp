// |jit-test| error: ReferenceError

// We elide the uninitialised lexical check for the second access to |x|,
// because we statically know that the first access will throw an error.
// `x ? . : .` is compiled straight from bytecode without going through any
// ICs, so already on first (eager) compilation we may be Warp compiling.

function testConditional(x = (x, (x ? 1 : 0))) {
    function inner() {
        // Close over |x|.
        x;
    }
}
testConditional();
