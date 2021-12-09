// |jit-test| skip-if: !('oomAtAllocation' in this) || helperThreadCount() === 0

let THREAD_TYPE_PARSE = 4;
let t = cacheEntry("function f() { function g() { }; return 3; };");
evaluate(t, { sourceIsLazy: true, saveIncrementalBytecode: true });
for (var i = 1; i < 20; ++i) {
    oomAtAllocation(i, THREAD_TYPE_PARSE);
    offThreadDecodeStencil(t, { sourceIsLazy: true });
    gc();
}
