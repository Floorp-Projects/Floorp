// |jit-test| --code-coverage

let x = cacheEntry("function inner() { return 3; }; inner()");
evaluate(x, { saveIncrementalBytecode: true });

try {
    // Fail XDR decode with partial script
    oomAtAllocation(20);
    evaluate(x, { loadBytecode: true });
} catch { }

getLcovInfo();
