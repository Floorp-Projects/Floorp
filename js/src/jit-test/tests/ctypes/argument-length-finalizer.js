load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(1); },
                         "CDataFinalizer constructor takes two arguments");

  let fin = ctypes.CDataFinalizer(ctypes.int32_t(0), ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [ctypes.int32_t]).ptr(x => x));
  assertTypeErrorMessage(() => { fin.forget(1); },
                         "CDataFinalizer.prototype.forget takes no arguments");
  assertTypeErrorMessage(() => { fin.dispose(1); },
                         "CDataFinalizer.prototype.dispose takes no arguments");
  fin.forget();
}

if (typeof ctypes === "object")
  test();
