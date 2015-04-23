load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.int32_t(0).address(1); },
                         "CData.prototype.address takes no arguments");
  assertTypeErrorMessage(() => { ctypes.char.array(10)().readString(1); },
                         "CData.prototype.readString takes no arguments");
  assertTypeErrorMessage(() => { ctypes.char.array(10)().readStringReplaceMalformed(1); },
                         "CData.prototype.readStringReplaceMalformed takes no arguments");
  assertTypeErrorMessage(() => { ctypes.int32_t(0).toSource(1); },
                         "CData.prototype.toSource takes no arguments");
}

if (typeof ctypes === "object")
  test();
