// Simple tests for getExceptionInfo behavior.
function testTestingFunction() {
    let vals = [{}, 1, "foo", null, undefined];
    for (let v of vals) {
        let thrower = () => { throw v; };
        let info = getExceptionInfo(thrower);
        assertEq(info.exception, v);
        assertEq(info.stack.includes("thrower@"), true);
    }

    // Returns null if there was no exception.
    assertEq(getExceptionInfo(() => 123), null);

    // OOM exceptions don't have a stack trace.
    let info = getExceptionInfo(throwOutOfMemory);
    assertEq(info.exception, "out of memory");
    assertEq(info.stack, null);
}
testTestingFunction();

/**
 * Check that the expected number of stack traces are generated for a given
 * global where 100 "throws" are generated
 */
function assertStacksCount(global, expectedStacksCount) {
    global.evaluate("(" + function(_expectedStacksCount) {
        let thrower = () => { throw 123; };
        for (let i = 0; i < 100; i++) {
            let info = getExceptionInfo(thrower);
            assertEq(info.exception, 123);
            // NOTE: if this ever gets increased, update the tests above too!
            if (i <= _expectedStacksCount) {
                assertEq(info.stack.includes("thrower@"), true);
            } else {
                assertEq(info.stack, null);
            }
        }
    } + `)(${expectedStacksCount})`);
}

// Debuggee globals always get an exception stack.
function testDebuggee() {
    let g = newGlobal({newCompartment: true});
    let dbg = new Debugger(g);
    assertStacksCount(g, 100);
}
testDebuggee();

// Globals with trusted principals always get an exception stack.
function testTrustedPrincipals() {
    let g = newGlobal({newCompartment: true, systemPrincipal: true});
    assertStacksCount(g, 100);
}
testTrustedPrincipals();

// In normal cases, a stack is captured only for the first 50 exceptions per realm.
function testNormal() {
    let g = newGlobal();
    assertStacksCount(g, 50);
}
testNormal();

// Non debuggee with unlimited stacks capturing enabled should always get a stack.
function testEnableUnlimitedStacksCapturing() {
    let dbg = new Debugger();
    let g = newGlobal();
    dbg.enableUnlimitedStacksCapturing(g);
    assertStacksCount(g, 100);

    dbg.disableUnlimitedStacksCapturing(g);
    assertStacksCount(g, 50);

    dbg.enableUnlimitedStacksCapturing(g);
    assertStacksCount(g, 100);
}
testEnableUnlimitedStacksCapturing();
