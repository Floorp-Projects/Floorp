// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

let THREAD_TYPE_PARSE = 4;

// Test main thread encode/decode OOM
oomTest(function() {
    let t = cacheEntry(`function f() { function g() { }; return 3; };`);

    evaluate(t, { sourceIsLazy: true, saveIncrementalBytecode: true });
    evaluate(t, { sourceIsLazy: true, readBytecode: true });
});

// Test helper thread decode OOM
let t = cacheEntry(`function f() { function g() { }; return 3; };`);
evaluate(t, { sourceIsLazy: true, saveIncrementalBytecode: true });
for (var i = 1; i < 100; ++i) {
    try {
        oomAtAllocation(i, THREAD_TYPE_PARSE);
        offThreadDecodeStencil(t, { sourceIsLazy: true });
        var stencil = finishOffThreadStencil();
        evalStencil(stencil);
    }
    catch (e) {
        assertEq(e, "out of memory");
    }
}
