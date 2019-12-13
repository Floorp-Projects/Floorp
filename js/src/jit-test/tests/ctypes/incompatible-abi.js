// |jit-test| skip-if: !this.ctypes || !ctypes.default_abi.toSource

load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.default_abi.toSource.call(1); },
                         "ABI.prototype.toSource called on incompatible object, got the number 1");
}

if (typeof ctypes === "object")
  test();
