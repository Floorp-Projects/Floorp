if (!wasmIsSupported())
    quit();

load(scriptdir + 'harness/index.js');
load(scriptdir + 'harness/wasm-constants.js');
load(scriptdir + 'harness/wasm-module-builder.js');

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
    func()
    .then(_ => {
        print(`${description}: PASS.`);
    })
    .catch(err => {
        print(`${description}: FAIL.
${err}`);
    });
    drainJobQueue();
}

let assert_equals = assertEq;
let assert_true = (x, errMsg) => { assertEq(x, true); }
let assert_false = (x, errMsg) => { assertEq(x, false); }

function assert_unreached(description) {
    throw new Error(`unreachable:\n${description}`);
}

setJitCompilerOption('wasm.test-mode', 1);
