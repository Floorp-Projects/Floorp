// |jit-test| skip-if: isLcovEnabled()

load(libdir + 'bytecode-cache.js');
var test = "";

// Check that without cloneSingleton, we can safely encode object literal which
// have mutations.
test = `
  var obj = { a: 1, b: 2 };
  obj.a++;
  assertEq(obj.a, 2);
`;
evalWithCache(test, {
  incremental: true,
  assertEqResult : true
});


// Encode lazy functions.
test = `
  function f() { return 1; };
  1;
`;
evalWithCache(test, {
  incremental: true,
  assertEqResult : true
});


// Encode delazified functions.
test = `
  function f() { return 1; };
  f();
`;
evalWithCache(test, {
  incremental: true,
  assertEqResult : true
});

// Encode inner functions.
test = `
  function g() {
    return function f() { return 1; };
  };
  g()();
`;
evalWithCache(test, {
  incremental: true,
  assertEqResult : true
});

// Extra zeal GCs can cause isRelazifiableFunction() to become true after we
// record its value by throwing away JIT code for the function.
gczeal(0);

// Relazified functions are encoded as non-lazy-script, when the encoding is
// incremental.
test = `
  assertEq(isLazyFunction(f), generation == 0 || generation == 3);
  function f() { return isRelazifiableFunction(f); };
  expect = f();
  assertEq(isLazyFunction(f), false);
  relazifyFunctions(f);
  assertEq(isLazyFunction(f), expect);
`;
evalWithCache(test, { incremental: true });
