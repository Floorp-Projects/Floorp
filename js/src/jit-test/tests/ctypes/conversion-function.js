// Type conversion error should report its type.

load(libdir + 'asserts.js');

function test() {
  // Note: js shell cannot handle the exception in return value.

  // primitive
  let func_type = ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t,
                                      [ctypes.int32_t]).ptr;
  let f1 = func_type(function() {});
  assertTypeErrorMessage(() => { f1("foo"); },
                         /can't pass the string "foo" to argument 1 of ctypes\.FunctionType\(ctypes\.default_abi, ctypes\.voidptr_t, \[ctypes\.int32_t\]\)\.ptr\(ctypes\.UInt64\("[x0-9A-Fa-f]+"\)\)/);

  // struct
  let test_struct = ctypes.StructType("test_struct", [{ "x": ctypes.int32_t }]);
  let func_type2 = ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t,
                                       [test_struct]).ptr;
  let f2 = func_type2(function() {});
  assertTypeErrorMessage(() => { f2({ "x": "foo" }); },
                         /can't convert the string \"foo\" to the 'x' field \(int32_t\) of test_struct at argument 1 of ctypes\.FunctionType\(ctypes\.default_abi, ctypes.int32_t, \[test_struct\]\)\.ptr\(ctypes\.UInt64\(\"[x0-9A-Fa-f]+\"\)\)/);
  assertTypeErrorMessage(() => { f2({ "x": "foo", "y": "bar" }); },
                         /property count of the object \(\{x:\"foo\", y:\"bar\"\}\) does not match to field count of the type test_struct \(expected 1, got 2\) at argument 1 of ctypes\.FunctionType\(ctypes\.default_abi, ctypes\.int32_t, \[test_struct\]\)\.ptr\(ctypes\.UInt64\(\"[x0-9A-Fa-f]+\"\)\)/);
  assertTypeErrorMessage(() => { f2({ 0: "foo" }); },
                         /property name the number 0 of the object \(\{0:\"foo\"\}\) is not a string at argument 1 of ctypes\.FunctionType\(ctypes\.default_abi, ctypes\.int32_t, \[test_struct\]\)\.ptr\(ctypes\.UInt64\(\"[x0-9A-Fa-f]+\"\)\)/);

  // error sentinel
  assertTypeErrorMessage(() => { func_type(function() {}, null, "foo"); },
                         "can't convert the string \"foo\" to the return type of ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t, [ctypes.int32_t])");
}

if (typeof ctypes === "object")
  test();
