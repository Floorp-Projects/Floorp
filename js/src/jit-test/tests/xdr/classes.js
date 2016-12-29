load(libdir + 'bytecode-cache.js');

// Prevent relazification triggered by some zeal modes. Relazification can cause
// test failures because lazy functions are XDR'd differently.
gczeal(0);

var test = "new class extends class { } { constructor() { super(); } }()";
evalWithCache(test, { assertEqBytecode : true });

var test = "new class { method() { super.toString(); } }().method()";
evalWithCache(test, { assertEqBytecode : true });
