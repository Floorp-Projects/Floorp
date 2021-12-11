// Return a promise that will resolve to `undefined` the next time jobs are
// processed.
//
// `ticks` indicates how long the promise should "wait" before resolving: a
// promise created with `asyncSleep(n)` will become settled and fire its handlers
// before a promise created with `asyncSleep(n+1)`.
//
function asyncSleep(ticks) {
    let p = Promise.resolve();
    if (ticks > 0) {
        return p.then(() => asyncSleep(ticks - 1));
    }
    return p;
}

// Run the async function `test`. Wait for it to finish running. Throw if it
// throws or if it fails to finish (awaiting a value forever).
function runAsyncTest(test) {
    let passed = false;
    let problem = "test did not finish";
    test()
        .then(_ => { passed = true; })
        .catch(exc => { problem = exc; });
    drainJobQueue();
    if (!passed) {
        throw problem;
    }

    reportCompare(0, 0);
}
