// Type conversion error should report its type.

load(libdir + 'asserts.js');

function test() {
  let test_struct = ctypes.StructType("test_struct", [{ "x": ctypes.int32_t }]);
  let struct_val = test_struct();

  // constructor
  assertTypeErrorMessage(() => { ctypes.int32_t.ptr("foo"); },
                         "can't convert the string \"foo\" to the type ctypes.int32_t.ptr");

  // value setter
  assertTypeErrorMessage(() => { test_struct.ptr().value = "foo"; },
                         "can't convert the string \"foo\" to the type test_struct.ptr");
  assertTypeErrorMessage(() => { test_struct.ptr().value = {}; },
                         "can't convert the object ({}) to the type test_struct.ptr");
  assertTypeErrorMessage(() => { test_struct.ptr().value = [1, 2]; },
                         "can't convert the array [1, 2] to the type test_struct.ptr");
  assertTypeErrorMessage(() => { test_struct.ptr().value = new Int8Array([1, 2]); },
                         "can't convert the typed array ({0:1, 1:2}) to the type test_struct.ptr");

  // contents setter
  assertTypeErrorMessage(() => { ctypes.int32_t().address().contents = {}; },
                         "can't convert the object ({}) to the type int32_t");
}

if (typeof ctypes === "object")
  test();
