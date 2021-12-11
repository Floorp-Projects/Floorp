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

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});

var source = new g.Uint8Array(wasmTextToBinary('(module (func unreachable))'));
g.source = source;

var dbg = new Debugger(g);
dbg.allowWasmBinarySource = true;
dbg.onNewScript = function (s, g) {
    drainJobQueue();
};

g.eval('WebAssembly.instantiateStreaming(source);');

drainJobQueue();
