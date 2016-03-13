load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.cast(ctypes.int32_t(0), ctypes.StructType("foo")); },
                         "target type foo has undefined size");

  assertTypeErrorMessage(() => { ctypes.cast(ctypes.int32_t(0), ctypes.StructType("foo", [ { x: ctypes.int32_t }, { y: ctypes.int32_t } ])); },
                         "target type foo has larger size than source type ctypes.int32_t (8 > 4)");
}

if (typeof ctypes === "object")
  test();
