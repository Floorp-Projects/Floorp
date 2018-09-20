if (!wasmStreamingIsSupported())
    quit();

const {Module, Instance, compileStreaming} = WebAssembly;

function testCached(code, imports, test) {
    if (typeof code === 'string')
        code = wasmTextToBinary(code);

    let success = false;
    let cache = streamCacheEntry(code);
    assertEq(cache.cached, false);
    compileStreaming(cache)
    .then(m => {
         test(new Instance(m, imports));
         while (!wasmHasTier2CompilationCompleted(m)) {
            sleep(1);
         }
         assertEq(cache.cached, wasmCachingIsSupported());
         return compileStreaming(cache);
     })
     .then(m => {
         test(new Instance(m, imports));
         assertEq(cache.cached, wasmCachingIsSupported());
         success = true;
     })
     .catch(err => { print(String(err) + " at:\n" + err.stack) });

     drainJobQueue();
     assertEq(success, true);
}

testCached(
    `(module
       (func (export "run") (result i32)
         (i32.const 42)))`,
    undefined,
    i => { assertEq(i.exports.run(), 42); }
);

testCached(
    `(module
       (type $T (func (result i32)))
       (func $t1 (import "" "t1") (type $T))
       (func $t2 (import "" "t2") (type $T))
       (func $t3 (type $T) (i32.const 30))
       (func $t4 (type $T) (i32.const 40))
       (table anyfunc (elem $t1 $t2 $t3 $t4))
       (func (export "run") (param i32) (result i32)
         (call_indirect $T (get_local 0))))`,
    {'':{ t1() { return 10 }, t2() { return 20 } }},
    i => {
        assertEq(i.exports.run(0), 10);
        assertEq(i.exports.run(1), 20);
        assertEq(i.exports.run(2), 30);
        assertEq(i.exports.run(3), 40);
    }
);

// Note: a fuller behavioral test of caching is in bench/wasm_box2d.js.
