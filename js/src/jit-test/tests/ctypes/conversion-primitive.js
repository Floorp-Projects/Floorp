// Type conversion error should report its type.

load(libdir + 'asserts.js');

function test() {
  // constructor
  assertTypeErrorMessage(() => { ctypes.int32_t("foo"); },
                         "can't convert the string \"foo\" to the type int32_t");
  assertTypeErrorMessage(() => { ctypes.int32_t(null); },
                         "can't convert null to the type int32_t");
  assertTypeErrorMessage(() => { ctypes.int32_t(undefined); },
                         "can't convert undefined to the type int32_t");
  assertTypeErrorMessage(() => { ctypes.int32_t({}); },
                         "can't convert the object ({}) to the type int32_t");
  assertTypeErrorMessage(() => { ctypes.int32_t([]); },
                         "can't convert the array [] to the type int32_t");
  assertTypeErrorMessage(() => { ctypes.int32_t(new Int8Array([])); },
                         "can't convert the typed array ({}) to the type int32_t");
  assertTypeErrorMessage(() => { ctypes.int32_t(ctypes.int32_t); },
                         "can't convert ctypes.int32_t to the type int32_t");
  assertRangeErrorMessage(() => { ctypes.int32_t("0xfffffffffffffffffffffff"); },
                          "the string \"0xfffffffffffffffffffffff\" does not fit in the type int32_t");
  if (typeof Symbol === "function") {
    assertTypeErrorMessage(() => { ctypes.int32_t(Symbol.iterator); },
                           "can't convert Symbol.iterator to the type int32_t");
    assertTypeErrorMessage(() => { ctypes.int32_t(Symbol("foo")); },
                           "can't convert Symbol(\"foo\") to the type int32_t");
  }

  // value setter
  let test_struct = ctypes.StructType("test_struct", [{ "x": ctypes.int32_t }]);
  let struct_val = test_struct();
  assertTypeErrorMessage(() => { ctypes.bool().value = struct_val; },
                         "can't convert test_struct(0) to the type boolean");
  assertTypeErrorMessage(() => { ctypes.char16_t().value = struct_val; },
                         "can't convert test_struct(0) to the type char16_t");
  assertTypeErrorMessage(() => { ctypes.int8_t().value = struct_val; },
                         "can't convert test_struct(0) to the type int8_t");
  assertTypeErrorMessage(() => { ctypes.double().value = struct_val; },
                         "can't convert test_struct(0) to the type double");
}

if (typeof ctypes === "object")
  test();
