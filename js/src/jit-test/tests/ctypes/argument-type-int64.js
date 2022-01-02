load(libdir + 'asserts.js');

function test() {
  assertRangeErrorMessage(() => { ctypes.Int64(0).toString("a"); },
                         "argument of Int64.prototype.toString must be an integer at least 2 and no greater than 36");
  assertTypeErrorMessage(() => { ctypes.Int64.compare(1, 2); },
                         "first argument of Int64.compare must be a Int64");
  assertTypeErrorMessage(() => { ctypes.Int64.compare(ctypes.Int64(0), 2); },
                         "second argument of Int64.compare must be a Int64");
  assertTypeErrorMessage(() => { ctypes.Int64.lo(1); },
                         "argument of Int64.lo must be a Int64");
  assertTypeErrorMessage(() => { ctypes.Int64.hi(1); },
                         "argument of Int64.hi must be a Int64");

  assertRangeErrorMessage(() => { ctypes.UInt64(0).toString("a"); },
                         "argument of UInt64.prototype.toString must be an integer at least 2 and no greater than 36");
  assertTypeErrorMessage(() => { ctypes.UInt64.compare(1, 2); },
                         "first argument of UInt64.compare must be a UInt64");
  assertTypeErrorMessage(() => { ctypes.UInt64.compare(ctypes.UInt64(0), 2); },
                         "second argument of UInt64.compare must be a UInt64");
  assertTypeErrorMessage(() => { ctypes.UInt64.lo(1); },
                         "argument of UInt64.lo must be a UInt64");
  assertTypeErrorMessage(() => { ctypes.UInt64.hi(1); },
                         "argument of UInt64.hi must be a UInt64");
}

if (typeof ctypes === "object")
  test();
