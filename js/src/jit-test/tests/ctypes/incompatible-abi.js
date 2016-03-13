load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.default_abi.toSource.call(1); },
                         "ABI.prototype.toSource called on incompatible Number");
}

if (typeof ctypes === "object")
  test();
