load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.int32_t.toString.call(1); },
                         "CType.prototype.toString called on incompatible Number");
  assertTypeErrorMessage(() => { ctypes.int32_t.toSource.call(1); },
                         "CType.prototype.toSource called on incompatible Number");
}

if (typeof ctypes === "object")
  test();
