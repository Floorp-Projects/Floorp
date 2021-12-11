load(libdir + 'bytecode-cache.js');

function test() {
  class A {
    #x;
  }
};

evalWithCache(test.toString(), { assertEqBytecode: true, assertEqResult: true });
