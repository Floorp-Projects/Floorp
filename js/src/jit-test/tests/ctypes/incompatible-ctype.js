load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.int32_t.toString.call(1); },
                         "CType.prototype.toString called on incompatible object, got the number 1");
  if (ctypes.int32_t.prototype.toSource) {
    assertTypeErrorMessage(() => { ctypes.int32_t.toSource.call(1); },
                           "CType.prototype.toSource called on incompatible object, got the number 1");
  }
}

if (typeof ctypes === "object")
  test();
