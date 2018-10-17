// |jit-test| skip-if: !('oomTest' in this && 'stackTest' in this) || helperThreadCount() === 0

// Check the we can run a stack test on all threads except worker threads while
// a worker thread is running a OOM test at the same time.
//
// This test case uses setSharedObject to "synchronize" between a worker thread
// and the main thread. This worker thread enters the oomTest section, while the
// main thread waits. Then the worker thread remains in the oomTest section
// until the main thread finish entering and existing its stackTest section.

setSharedObject(0);
evalInWorker(`
    oomTest(() => {
        if (getSharedObject() < 2) {
            setSharedObject(1);
            while (getSharedObject() != 2) {
            }
        }
    });
`);

while (getSharedObject() != 1) {
    // poor-man wait condition.
}

stackTest(() => 42);
setSharedObject(2);
