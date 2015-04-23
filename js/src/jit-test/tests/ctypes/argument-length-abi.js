load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.default_abi.toSource(1); },
                         "ABI.prototype.toSource takes no arguments");
}

if (typeof ctypes === "object")
  test();
