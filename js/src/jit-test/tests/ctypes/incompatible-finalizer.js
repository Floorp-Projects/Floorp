load(libdir + 'asserts.js');

function test() {
  let fin = ctypes.CDataFinalizer(ctypes.int32_t(0), ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [ctypes.int32_t]).ptr(x => x));
  assertTypeErrorMessage(() => { fin.toSource.call(1); },
                         "CDataFinalizer.prototype.toSource called on incompatible Number");
  assertTypeErrorMessage(() => { fin.toString.call(1); },
                         "CDataFinalizer.prototype.toString called on incompatible Number");
  assertTypeErrorMessage(() => { fin.forget.call(1); },
                         "CDataFinalizer.prototype.forget called on incompatible object, got the object (new Number(1))");
  assertTypeErrorMessage(() => { fin.dispose.call(1); },
                         "CDataFinalizer.prototype.dispose called on incompatible object, got the object (new Number(1))");
  fin.forget();

  assertTypeErrorMessage(() => { fin.readString(); },
                         "CDataFinalizer.prototype.readString called on empty CDataFinalizer");
  assertTypeErrorMessage(() => { fin.dispose(); },
                         "CDataFinalizer.prototype.dispose called on empty CDataFinalizer");
  assertTypeErrorMessage(() => { fin.forget(); },
                         "CDataFinalizer.prototype.forget called on empty CDataFinalizer");
}

if (typeof ctypes === "object")
  test();
