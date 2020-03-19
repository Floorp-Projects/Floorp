// |jit-test| skip-if: !wasmCachingIsSupported()

load(libdir + "wasm-binary.js");

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
         assertEq(wasmLoadedFromCache(m), false);
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

testCached(`(module
    (func $test (param i64) (result f64)
        local.get 0
        f64.convert_u/i64
    )
    (func (export "run") (result i32)
        i64.const 1
        call $test
        f64.const 1
        f64.eq
    )
)`,
    undefined,
    i => { assertEq(i.exports.run(), 1); }
);

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
       (table funcref (elem $t1 $t2 $t3 $t4))
       (func (export "run") (param i32) (result i32)
         (call_indirect $T (local.get 0))))`,
    {'':{ t1() { return 10 }, t2() { return 20 } }},
    i => {
        assertEq(i.exports.run(0), 10);
        assertEq(i.exports.run(1), 20);
        assertEq(i.exports.run(2), 30);
        assertEq(i.exports.run(3), 40);
    }
);

testCached(
  moduleWithSections([
    sigSection([{args:[], ret:VoidCode}]),
    declSection([0]),
    exportSection([{funcIndex:0, name:"run"}]),
    bodySection([funcBody({locals:[], body:[UnreachableCode]})]),
    nameSection([funcNameSubsection([{name:"wee"}])])
  ]),
  undefined,
  i => assertErrorMessage(() => i.exports.run(), RuntimeError, /unreachable/)
);

// Note: a fuller behavioral test of caching is in bench/wasm_box2d.js.
