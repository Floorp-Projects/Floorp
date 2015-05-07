load(libdir + 'bytecode-cache.js');
var test = "";
var checkAfter;

// code a function which has both used and unused inner functions.
test = (function () {
  function f(x) {
    function ifTrue() {
      return true;
    };
    function ifFalse() {
      return false;
    };

    if (x) return ifTrue();
    else return ifFalse();
  }

  return f.toSource() + "; f(true)";
})();
evalWithCache(test, { assertEqBytecode: true, assertEqResult : true });

// code a function which uses different inner functions based on the generation.
test = (function () {
  function f(x) {
    function ifTrue() {
      return true;
    };
    function ifFalse() {
      return false;
    };

    if (x) return ifTrue();
    else return ifFalse();
  }

  return f.toSource() + "; f((generation % 2) == 0)";
})();
evalWithCache(test, { });

// Code a function which has an enclosing scope.
test = (function () {
  function f() {
    var upvar = "";
    function g() { upvar += ""; return upvar; }
    return g;
  }

  return f.toSource() + "; f()();";
})();
evalWithCache(test, { assertEqBytecode: true, assertEqResult : true });

// Code a lazy function which has an enclosing scope.
test = (function () {
  function f() {
    var upvar = "";
    function g() { upvar += ""; return upvar; }
    return g;
  }

  return f.toSource() + "; f();";
})();
evalWithCache(test, { assertEqBytecode: true });

// (basic/bug535930) Code an enclosing scope which is a Call object.
test = (function () {
  return "(" + (function () {
    p = function () {
        Set()
    };
    var Set = function () {};
    for (var x = 0; x < 5; x++) {
      Set = function (z) {
        return function () {
          [z]
        }
      } (x)
    }
  }).toSource() + ")()";
})();
evalWithCache(test, { assertEqBytecode: true });

// Code an arrow function, and execute it.
test = (function () {
  function f() {
    var g = (a) => a + a;
    return g;
  }

  return f.toSource() + "; f()(1);";
})();
evalWithCache(test, { assertEqBytecode: true, assertEqResult : true });

// Code an arrow function, and do not execute it.
test = (function () {
  function f() {
    var g = (a) => a + a;
    return g;
  }

  return f.toSource() + "; f();";
})();
evalWithCache(test, { assertEqBytecode: true });

// Extra zeal GCs can cause isRelazifiableFunction() to become true after we
// record its value by throwing away JIT code for the function.
gczeal(0);

// Ensure that decoded functions can be relazified.
test = "function f() { }; f();"
     + "assertEq(isLazyFunction(f), false);"
     + "var expect = isRelazifiableFunction(f);";
checkAfter = function (ctx) {
  gc(ctx.global.f); // relazify f, if possible.
  evaluate("assertEq(isLazyFunction(f), expect);", ctx);
};
evalWithCache(test, {
  assertEqBytecode: true,  // Check that we re-encode the same thing.
  assertEqResult: true,    // The function should remain relazifiable, if it was
                           // during the first run.
  checkAfter: checkAfter   // Check that relazifying the restored function works
                           // if the original was relazifiable.
});

// Ensure that decoded functions can be relazified, even if they have free
// variables.
test = "function f() { return isRelazifiableFunction(f) }; var expect = f();"
     + "assertEq(isLazyFunction(f), false);"
     + "expect";
checkAfter = function (ctx) {
  gc(ctx.global.f); // relazify f, if possible.
  evaluate("assertEq(isLazyFunction(f), expect);", ctx);
};
evalWithCache(test, {
  assertEqBytecode: true,  // Check that we re-encode the same thing.
  assertEqResult: true,    // The function should remain relazifiable, if it was
                           // during the first run.
  checkAfter: checkAfter   // Check that relazifying the restored function works
                           // if the original was relazifiable.
});

// Ensure that if a function is encoded when non-lazy but relazifiable, then
// decoded, relazified, and then delazified, the result actually works.
test = `
  function f() { return true; };
  var canBeLazy = isRelazifiableFunction(f) || isLazyFunction(f);
  relazifyFunctions();
  assertEq(isLazyFunction(f), canBeLazy);
  f()`
evalWithCache(test, { assertEqBytecode: true, assertEqResult: true });

// And more of the same, in a slightly different way
var g1 = newGlobal();
var g2 = newGlobal();
var res = "function f(){}";
var code = cacheEntry(res + "; f();");
evaluate(code, {global:g1, compileAndGo: true, saveBytecode: {value: true}});
evaluate(code, {global:g2, loadBytecode: true});
gc();
assertEq(g2.f.toString(), res);

// Another relazification case.
var src = "function f() { return 3; }; f(); relazifyFunctions(); 4";
evalWithCache(src, {assertEqBytecode: true, assertEqResult: true});
