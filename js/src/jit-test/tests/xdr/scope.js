load(libdir + 'bytecode-cache.js');
var test = "";

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
evalWithCache(test, { assertEqBytecode: true, assertEqResult : true });
