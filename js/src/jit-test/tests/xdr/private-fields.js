// |jit-test| --enable-private-fields;
load(libdir + 'bytecode-cache.js');

function test() {
  class A {
    #x;
  }
};

evalWithCache(test.toString(), {assertEqBytecode: true, assertEqResult: true});
