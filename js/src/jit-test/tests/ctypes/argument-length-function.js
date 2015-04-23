load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.FunctionType(); },
                         "FunctionType takes two or three arguments");
  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.void_t, []).ptr({}, 1); },
                         "FunctionType constructor takes one argument");
}

if (typeof ctypes === "object")
  test();
