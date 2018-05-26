if (!wasmIsSupported())
    quit();

// We need to find the absolute path that ends like this:
//
//  js/src/jit-test/tests/wasm/spec/harness/
//
// because that's where the test harness lives.  Fortunately we are provided
// with |libdir|, which is a path that ends thusly
//
//  js/src/jit-test/lib/
//
// That is, it has a fixed offset relative to what we need.  So we can
// simply do this:

let harnessdir = libdir + "../tests/wasm/spec/harness/";

load(harnessdir + 'index.js');
load(harnessdir + 'wasm-constants.js');
load(harnessdir + 'wasm-module-builder.js');

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
let assert_true = (x, errMsg) => { assertEq(x, true); }
let assert_false = (x, errMsg) => { assertEq(x, false); }

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
