// Accessing `value` property of non primitive type should report its type.

load(libdir + 'asserts.js');

function test() {
  let test_struct = ctypes.StructType("test_struct", [{ "x": ctypes.voidptr_t }]);
  assertTypeErrorMessage(() => test_struct().value,
                         ".value only works on character and numeric types, not `test_struct`");

  let test_array = ctypes.ArrayType(test_struct);
  assertTypeErrorMessage(() => test_array(10).value,
                         ".value only works on character and numeric types, not `test_struct.array(10)`");

  let test_pointer = ctypes.PointerType(test_struct);
  assertTypeErrorMessage(() => test_pointer(10).value,
                         ".value only works on character and numeric types, not `test_struct.ptr`");
}

if (typeof ctypes === "object")
  test();
