// |jit-test| skip-if: !('oomAfterAllocations' in this)

ignoreUnhandledRejections();

try {
    WebAssembly.compileStreaming();
} catch (err) {
    assertEq(String(err).indexOf("not supported with --no-threads") !== -1, true);
    quit();
}
oomAfterAllocations(1, 2);
var x = wasmTextToBinary('(module (func (export "run") (result i32) i32.const 42))');
WebAssembly.compileStreaming(x);
drainJobQueue();
