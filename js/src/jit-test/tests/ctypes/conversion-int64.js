load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.Int64("0xfffffffffffffffffffffff"); },
                         "can't pass the string \"0xfffffffffffffffffffffff\" to argument 1 of Int64");
  assertTypeErrorMessage(() => { ctypes.Int64.join("foo", 0); },
                         "can't pass the string \"foo\" to argument 1 of Int64.join");
  assertTypeErrorMessage(() => { ctypes.Int64.join(0, "foo"); },
                         "can't pass the string \"foo\" to argument 2 of Int64.join");

  assertTypeErrorMessage(() => { ctypes.UInt64("0xfffffffffffffffffffffff"); },
                         "can't pass the string \"0xfffffffffffffffffffffff\" to argument 1 of UInt64");
  assertTypeErrorMessage(() => { ctypes.UInt64.join("foo", 0); },
                         "can't pass the string \"foo\" to argument 1 of UInt64.join");
  assertTypeErrorMessage(() => { ctypes.UInt64.join(0, "foo"); },
                         "can't pass the string \"foo\" to argument 2 of UInt64.join");
}

if (typeof ctypes === "object")
  test();
