if (!wasmIsSupported())
    quit();

function test(func, description) {
    let maybeErr;
    try {
        func();
    } catch(e) {
        maybeErr = e;
    }

    if (typeof maybeErr !== 'undefined') {
        throw new Error(`${description}: FAIL.
${maybeErr}
${maybeErr.stack}`);
    } else {
        print(`${description}: PASS.`);
    }
}

function promise_test(func, description) {
    let maybeError = null;
    func()
    .then(_ => {
        print(`${description}: PASS.`);
    })
    .catch(err => {
        print(`${description}: FAIL.
${err}`);
        maybeError = err;
    });
    drainJobQueue();
    if (maybeError)
        throw maybeError;
}

let assert_equals = assertEq;
let assert_true = (x, errMsg) => { assertEq(x, true, errMsg || ''); }
let assert_false = (x, errMsg) => { assertEq(x, false, errMsg || ''); }

function assert_unreached(description) {
    throw new Error(`unreachable:\n${description}`);
}

function assert_not_equals(actual, not_expected, description) {
    let caught = false;
    try {
        assertEq(actual, not_expected, description);
    } catch (e) {
        caught = true;
    };
    assertEq(caught, true, "assert_not_equals failed: " + description);
}

// Make it possible to run wasm spec tests with --fuzzing-safe
if (typeof console == 'undefined') {
    console = { log() {} }
}
