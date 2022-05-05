const {Module, Instance, compileStreaming, RuntimeError} = WebAssembly;

function testCached(code, imports, test) {
    if (typeof code === 'string')
        code = wasmTextToBinary(code);

    let success = false;
    let cache = streamCacheEntry(code);
    assertEq(cache.cached, false);
    compileStreaming(cache)
    .then(m => {
         test(new Instance(m, imports));
         if (!wasmTestSerializationEnabled()) {
             assertEq(wasmLoadedFromCache(m), false);
         }
         while (!wasmHasTier2CompilationCompleted(m)) {
            sleep(1);
         }
         assertEq(cache.cached, true);
         return compileStreaming(cache);
     })
     .then(m => {
         test(new Instance(m, imports));
         assertEq(wasmLoadedFromCache(m), true);
         assertEq(cache.cached, true);

         let m2 = wasmCompileInSeparateProcess(code);
         test(new Instance(m2, imports));
         assertEq(wasmLoadedFromCache(m2), true);

         success = true;
     })
     .catch(err => { print(String(err) + " at:\n" + err.stack) });

     drainJobQueue();
     assertEq(success, true);
}
