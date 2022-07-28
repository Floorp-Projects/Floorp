function getStack() {
    enableGeckoProfiling();
    let stack = readGeckoProfilingStack();
    // The number of frames depends on JIT flags, but there must be at least
    // one frame for the caller and at most 3 total (the global script, 'testFun'
    // and 'getStack').
    assertEq(stack.length > 0, true);
    assertEq(stack.length <= 3, true);
    assertEq(JSON.stringify(stack).includes('"testFun ('), true);
    disableGeckoProfiling();
}
function testFun() {
    // Loop until this is a JIT frame.
    while (true) {
        let isJitFrame = inJit();
        if (typeof isJitFrame === "string") {
            return; // JIT disabled.
        }
        if (isJitFrame) {
            break;
        }
    }

    // Now call getStack to check this frame is on the profiler's JIT stack.
    getStack();
    getStack();
}
testFun();
