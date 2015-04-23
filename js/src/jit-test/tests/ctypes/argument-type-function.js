load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, 1); },
                         "third argument of FunctionType must be an array");
}

if (typeof ctypes === "object")
  test();
