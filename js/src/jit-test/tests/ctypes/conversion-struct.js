// Type conversion error should report its type.

load(libdir + 'asserts.js');

function test() {
  let test_struct = ctypes.StructType("test_struct", [{ "x": ctypes.int32_t },
                                                      { "bar": ctypes.int32_t }]);

  // constructor
  assertTypeErrorMessage(() => { new test_struct("foo"); },
                         "can't convert the string \"foo\" to the type test_struct");
  assertTypeErrorMessage(() => { new test_struct("foo", "x"); },
                         "can't convert the string \"foo\" to the 'x' field (int32_t) of test_struct");
  assertTypeErrorMessage(() => { new test_struct({ "x": "foo", "bar": 1 }); },
                         "can't convert the string \"foo\" to the 'x' field (int32_t) of test_struct");
  assertTypeErrorMessage(() => { new test_struct({ 0: 1, "bar": 1 }); },
                         "property name the number 0 of the object ({0:1, bar:1}) is not a string");

  // field setter
  let struct_val = test_struct();
  assertTypeErrorMessage(() => { struct_val.x = "foo"; },
                         "can't convert the string \"foo\" to the 'x' field (int32_t) of test_struct");
  assertTypeErrorMessage(() => { struct_val.bar = "foo"; },
                         "can't convert the string \"foo\" to the 'bar' field (int32_t) of test_struct");

  // value setter
  assertTypeErrorMessage(() => { struct_val.value = { "x": "foo" }; },
                         "property count of the object ({x:\"foo\"}) does not match to field count of the type test_struct (expected 2, got 1)");
  assertTypeErrorMessage(() => { struct_val.value = { "x": "foo", "bar": 1 }; },
                         "can't convert the string \"foo\" to the 'x' field (int32_t) of test_struct");
  assertTypeErrorMessage(() => { struct_val.value = "foo"; },
                         "can't convert the string \"foo\" to the type test_struct");
}

if (typeof ctypes === "object")
  test();
