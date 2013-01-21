// |jit-test| error:Error

// Binary: cache/js-dbg-64-85e31a4bdd41-linux
// Flags:
//

function f(x = let([] = c) 1, ... __call__)  {}
assertEq(this > f, true);
