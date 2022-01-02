// Type conversion error should report its type.

load(libdir + 'asserts.js');

function test() {
  // constructor
  assertTypeErrorMessage(() => { ctypes.int32_t.array()("foo"); },
                         "can't convert the string \"foo\" to the type ctypes.int32_t.array()");
  assertTypeErrorMessage(() => { ctypes.int32_t.array(10)("foo"); },
                         "can't convert the string \"foo\" to the type ctypes.int32_t.array(10)");
  assertTypeErrorMessage(() => { ctypes.char.array(2)("foo"); },
                         "length of the string \"foo\" does not fit in the length of the type ctypes.char.array(2) (expected 2 or lower, got 3)");
  assertTypeErrorMessage(() => { ctypes.char16_t.array(2)("foo"); },
                         "length of the string \"foo\" does not fit in the length of the type ctypes.char16_t.array(2) (expected 2 or lower, got 3)");
  assertTypeErrorMessage(() => { ctypes.int8_t.array(2)(new ArrayBuffer(8)); },
                         "length of the array buffer ({}) does not match to the length of the type ctypes.int8_t.array(2) (expected 2, got 8)");
  assertTypeErrorMessage(() => { ctypes.int8_t.array(2)(new Int8Array(8)); },
                         "length of the typed array ({0:0, 1:0, 2:0, 3:0, 4:0, 5:0, 6:0, 7:0}) does not match to the length of the type ctypes.int8_t.array(2) (expected 2, got 8)");

  // elem setter
  assertTypeErrorMessage(() => { ctypes.int32_t.array(10)()[0] = "foo"; },
                         "can't convert the string \"foo\" to element 0 of the type ctypes.int32_t.array(10)");
  assertTypeErrorMessage(() => { ctypes.int32_t.array(10)()[1] = "foo"; },
                         "can't convert the string \"foo\" to element 1 of the type ctypes.int32_t.array(10)");

  // value setter
  assertTypeErrorMessage(() => { ctypes.int32_t.array(1)().value = ["foo"]; },
                         "can't convert the string \"foo\" to element 0 of the type ctypes.int32_t.array(1)");
  assertTypeErrorMessage(() => { ctypes.int32_t.array(1)().value = [2, "foo"]; },
                         "length of the array [2, \"foo\"] does not match to the length of the type ctypes.int32_t.array(1) (expected 1, got 2)");
  assertTypeErrorMessage(() => { ctypes.int32_t.array(2)().value = [2, "foo"]; },
                         "can't convert the string \"foo\" to element 1 of the type ctypes.int32_t.array(2)");
}

if (typeof ctypes === "object")
  test();
