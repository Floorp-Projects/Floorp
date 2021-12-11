
let entry = cacheEntry(`
    function f() { }
    f()                 // Delazify
    f                   // Return function object
`);

let f1 = evaluate(entry, {saveIncrementalBytecode: true});
let f2 = evaluate(entry, {loadBytecode: true});

// Reading from XDR should still deduplicate to same bytecode
assertEq(hasSameBytecodeData(f1, f2), true)
