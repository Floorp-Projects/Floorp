// |jit-test| skip-if: !('oomTest' in this && 'stackTest' in this) || helperThreadCount() === 0

// Check that oomTest throws an exception on worker threads.

setSharedObject(0);
evalInWorker(`
    try {
        oomTest(crash);
    } catch (e) {
        if (e.toString().includes("main thread")) {
            setSharedObject(1);
        }
    }
`);

while (getSharedObject() != 1) {
    // poor-man wait condition.
}
