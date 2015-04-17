load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.cast(1, 2); },
                         "first argument of ctypes.cast must be a CData");
  assertTypeErrorMessage(() => { ctypes.cast(ctypes.int32_t(0), 2); },
                         "second argument of ctypes.cast must be a CType");
  assertTypeErrorMessage(() => { ctypes.getRuntime(1); },
                         "argument of ctypes.getRuntime must be a CType");
}

if (typeof ctypes === "object")
  test();
