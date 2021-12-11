// |jit-test| skip-if: !wasmDebuggingEnabled()
// Draining the job queue from an onNewScript hook reporting a streamed wasm
// module should not deadlock.

ignoreUnhandledRejections();

try {
    WebAssembly.compileStreaming();
    // Avoid mixing the test's jobs with the debuggee's, so that
    // automated checks can make sure AutoSaveJobQueue only
    // suspends debuggee work.
    drainJobQueue();
} catch (err) {
    assertEq(String(err).indexOf("not supported with --no-threads") !== -1, true);
    quit();
}

var g = newGlobal({newCompartment: true});

var source = new g.Uint8Array(wasmTextToBinary('(module (func unreachable))'));
g.source = source;

var dbg = new Debugger(g);
dbg.allowWasmBinarySource = true;
dbg.onNewScript = function (s, g) {
  drainJobQueue();
};

// For the old code to deadlock, OffThreadPromiseRuntimeState::internalDrain
// needs to get two Dispatchables at once when it swaps queues. A call to
// instantiateStreaming enqueues a job that will kick off a helper thread, so to
// make sure that two helper threads have had time to complete and enqueue their
// Dispatchables, we put a delay in the job queue after the helper thread
// launches.
g.eval(`
  WebAssembly.instantiateStreaming(source);
  WebAssembly.instantiateStreaming(source);
  Promise.resolve().then(() => sleep(0.1));
`);

drainJobQueue();
