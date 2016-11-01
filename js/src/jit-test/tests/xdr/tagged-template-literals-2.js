var code = cacheEntry("(x => x.toSource())`bar`;");
var g = newGlobal({ cloneSingletons: true });
assertEq("[\"bar\"]", evaluate(code, { global: g, saveBytecode: true }));
assertEq("[\"bar\"]", evaluate(code, { global: g, loadBytecode: true }));
