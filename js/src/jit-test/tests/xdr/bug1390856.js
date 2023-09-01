// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

// Test main thread encode/decode OOM
oomTest(function() {
    let t = cacheEntry(`function f() { function g() { }; return 3; };`);

    evaluate(t, { sourceIsLazy: true, saveIncrementalBytecode: true });
    evaluate(t, { sourceIsLazy: true, readBytecode: true });
});
