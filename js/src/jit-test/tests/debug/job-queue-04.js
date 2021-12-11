// |jit-test| skip-if: !('oomTest' in this)
// Bug 1527862: Don't assert that the Debugger drained its job queue unless we
// actually saved the debuggee's queue.

// Put a job in the queue.
Promise.resolve(42).then(() => {});

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
dbg.onNewScript = script => {};

// Cause an OOM while initializing the AutoDebuggerJobQueueInterruption, so that
// the destructor is run on an uninitialized instance.
//
// A properly initialized AutoDebuggerJobQueueInterruption asserts that the
// debugger left its job queue entry, before restoring the debuggee's job queue
// that it saved when it was initialized. But if OOM interrupts initialization,
// the job queue left on the JSContext is still the debuggee's, which we have no
// reason to expect is empty, so we shouldn't make any assertions about its
// state. The assertion must be conditional on proper initialization (and use
// the correct condition).
oomTest(() => {
    g.eval("(function() {})");
}, {expectExceptionOnFailure: false});
