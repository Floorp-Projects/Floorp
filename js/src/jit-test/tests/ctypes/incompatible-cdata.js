load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.int32_t(0).address.call(1); },
                         "CData.prototype.address called on incompatible object, got the object (new Number(1))");
  assertTypeErrorMessage(() => { ctypes.char.array(10)("abc").readString.call(1); },
                         "CData.prototype.readString called on incompatible object, got the object (new Number(1))");

  assertTypeErrorMessage(() => { ctypes.char.array(10)("abc").readStringReplaceMalformed.call(1); },
                         "CData.prototype.readStringReplaceMalformed called on incompatible object, got the object (new Number(1))");
  assertTypeErrorMessage(() => { ctypes.int32_t(0).toSource.call(1); },
                         "CData.prototype.toSource called on incompatible Number");

  let p = Object.getPrototypeOf(ctypes.int32_t());
  let o = {};
  Object.setPrototypeOf(o, p);
  assertTypeErrorMessage(() => { o.readString(); },
                         "CData.prototype.readString called on incompatible object, got <<error converting value to string>>");
  assertTypeErrorMessage(() => { o.readStringReplaceMalformed(); },
                         "CData.prototype.readStringReplaceMalformed called on incompatible object, got <<error converting value to string>>");
}

if (typeof ctypes === "object")
  test();
