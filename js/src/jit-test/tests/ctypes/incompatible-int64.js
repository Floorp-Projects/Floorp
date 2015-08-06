load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.Int64(0).toString.call(1); },
                         "Int64.prototype.toString called on incompatible Number");
  assertTypeErrorMessage(() => { ctypes.Int64(0).toString.call(ctypes.int32_t(0)); },
                         "Int64.prototype.toString called on non-Int64 CData");
  assertTypeErrorMessage(() => { ctypes.Int64(0).toSource.call(1); },
                         "Int64.prototype.toSource called on incompatible Number");
  assertTypeErrorMessage(() => { ctypes.Int64(0).toSource.call(ctypes.int32_t(0)); },
                         "Int64.prototype.toSource called on non-Int64 CData");

  assertTypeErrorMessage(() => { ctypes.UInt64(0).toString.call(1); },
                         "UInt64.prototype.toString called on incompatible Number");
  assertTypeErrorMessage(() => { ctypes.UInt64(0).toString.call(ctypes.int32_t(0)); },
                         "UInt64.prototype.toString called on non-UInt64 CData");
  assertTypeErrorMessage(() => { ctypes.UInt64(0).toSource.call(1); },
                         "UInt64.prototype.toSource called on incompatible Number");
  assertTypeErrorMessage(() => { ctypes.UInt64(0).toSource.call(ctypes.int32_t(0)); },
                         "UInt64.prototype.toSource called on non-UInt64 CData");
}

if (typeof ctypes === "object")
  test();
