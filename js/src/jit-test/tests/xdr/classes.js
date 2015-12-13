load(libdir + 'bytecode-cache.js');

var test = "new class extends class { } { constructor() { super(); } }()";
evalWithCache(test, { assertEqBytecode : true });

var test = "new class { method() { super.toString(); } }().method()";
evalWithCache(test, { assertEqBytecode : true });
