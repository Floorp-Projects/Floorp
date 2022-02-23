// |jit-test| skip-if: isLcovEnabled()

// Self-hosted builtins use a special form of lazy function, but still can be
// delazified in some cases.

let obj = [];
let fun = obj.map;
assertEq(isLazyFunction(fun), true);

// Delazify
fun.call(obj, x => x);
assertEq(isLazyFunction(fun), false);

// Relazify
relazifyFunctions(obj);
assertEq(isLazyFunction(fun), true);
