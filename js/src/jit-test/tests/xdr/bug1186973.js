// |jit-test| error: cache does not have the same size
load(libdir + 'bytecode-cache.js');

var test = (function () {
  function f(x) {
    function ifTrue() {};
    function ifFalse() {};

    if (generation % 2 == 0)
      return ifTrue();
    return ifFalse();
  }
  return f.toSource() + "; f()";
})();
evalWithCache(test, { assertEqBytecode: true, assertEqResult : true });
