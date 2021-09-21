// |jit-test| error: TypeError

var global = newGlobal({ cloneSingletons: true });

var code = cacheEntry(`
function f() {}
Object.freeze(this);
`);

evaluate(code, { global, saveIncrementalBytecode: true });
evaluate(code, { global, saveIncrementalBytecode: true });
