load(libdir + 'bytecode-cache.js');
load(libdir + 'class.js');

if (!classesEnabled())
    quit();

var test = "new class extends class { } { constructor() { super(); } }()";
evalWithCache(test, { assertEqBytecode : true });

var test = "new class { method() { super.toString(); } }().method()";
evalWithCache(test, { assertEqBytecode : true });
