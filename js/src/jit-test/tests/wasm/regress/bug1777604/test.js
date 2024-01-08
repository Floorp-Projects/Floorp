// |jit-test| skip-if: !wasmStreamingEnabled()

const { Instance, compile, compileStreaming } = WebAssembly;

for (let i = 0; i < 200; i++) {
    // Possibly free code from a previous iteration, increasing probability
    // new code will be allocated in a location previous code was.
    gc(undefined, "shrinking");

    function compileAndRun(x, g) {
        var cache = streamCacheEntry(wasmTextToBinary(x));

        compileStreaming(cache).then(function (m) {
            g(new Instance(m, undefined));
            return compileStreaming(cache);
        }).then(function (m) {
            g(new Instance(m, undefined));
        });

        // Serialize the promise execution at this point
        drainJobQueue();
    }

    // Compile two different modules to increase probability that a module is
    // is compiled to a page that a different module resided in previously.
    compileAndRun('(module \
        (func $test (param i64) (result f64) local.get 0 f64.convert_i64_u )\
        (func (export "run") (result i32) i64.const 1 call $test f64.const 1 f64.eq )\
    )', function (i) {});
    compileAndRun('(module \
        (func (export "run") (result i64) (i64.const 1))\
    )', function (i) {
        i.exports.run();
    });
}
