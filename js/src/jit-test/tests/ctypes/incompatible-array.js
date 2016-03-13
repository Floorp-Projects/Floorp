load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.int32_t.array.call(1); },
                         "CType.prototype.array called on incompatible object, got the object (new Number(1))");
  assertTypeErrorMessage(() => { ctypes.int32_t.array(10)().addressOfElement.call(1); },
                         "ArrayType.prototype.addressOfElement called on incompatible object, got the object (new Number(1))");
  assertTypeErrorMessage(() => { ctypes.int32_t.array(10)().addressOfElement.call(ctypes.int32_t(0)); },
                         "ArrayType.prototype.addressOfElement called on non-ArrayType CData, got ctypes.int32_t(0)");
}

if (typeof ctypes === "object")
  test();
