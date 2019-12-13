var code = cacheEntry("(x => JSON.stringify(x))`bar`;");
var g = newGlobal({ cloneSingletons: true });
assertEq("[\"bar\"]", evaluate(code, { global: g, saveBytecode: true }));
assertEq("[\"bar\"]", evaluate(code, { global: g, loadBytecode: true }));
