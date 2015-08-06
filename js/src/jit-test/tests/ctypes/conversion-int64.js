load(libdir + 'asserts.js');

function test() {
  assertRangeErrorMessage(() => { ctypes.Int64("0xfffffffffffffffffffffff"); },
                          "the string \"0xfffffffffffffffffffffff\" does not fit in the type int64");
  assertTypeErrorMessage(() => { ctypes.Int64.join("foo", 0); },
                         "can't pass the string \"foo\" to argument 1 of Int64.join");
  assertTypeErrorMessage(() => { ctypes.Int64.join(0, "foo"); },
                         "can't pass the string \"foo\" to argument 2 of Int64.join");

  assertRangeErrorMessage(() => { ctypes.UInt64("0xfffffffffffffffffffffff"); },
                          "the string \"0xfffffffffffffffffffffff\" does not fit in the type uint64");
  assertTypeErrorMessage(() => { ctypes.UInt64.join("foo", 0); },
                         "can't pass the string \"foo\" to argument 1 of UInt64.join");
  assertTypeErrorMessage(() => { ctypes.UInt64.join(0, "foo"); },
                         "can't pass the string \"foo\" to argument 2 of UInt64.join");
}

if (typeof ctypes === "object")
  test();
