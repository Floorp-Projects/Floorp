load(libdir + 'bytecode-cache.js');
var test = "";

// code a nested function after calling it
test = `
    function f() {
        function g() {
            return [f, g];
        }
        return g();
    }
    f()
`;
evalWithCache(test, {
    assertEqBytecode: true,
    checkAfter(ctx) {
        let [f, g] = ctx.global.f();
        assertEq(f, ctx.global.f);
        assertEq(f()[0], f);
        assertEq(f()[1] === g, false); // second call, fresh g closure
        assertEq(f()[1].toString(), g.toString()); // but the same source code
        assertEq(g()[0], f);
        assertEq(g()[1], g);
    }
});

// code an unused function that contains an unused nested function
test = `
    function f() {
        function g() {
            return [f, g];
        }
        return g;
    }
    f
`;
evalWithCache(test, {
    assertEqBytecode: true,
    checkAfter(ctx) {
        let f = ctx.global.f;
        let g = f();
        let [f1, g1] = g();
        assertEq(f1, f);
        assertEq(g1, g);
    }
});

// code a function which has both used and unused inner functions.
test  = (function () {
  function f() {
    var x = 3;
    (function() {
      with(obj) {
        (function() {
          assertEq(x, 2);
        })();
      }
    })();
  };

  return "var obj = { x : 2 };" + f.toSource() + "; f()";
})();
evalWithCache(test, { assertEqBytecode: true, assertEqResult: true });
