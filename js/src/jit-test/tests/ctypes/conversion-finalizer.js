// |jit-test| skip-if: getBuildConfiguration("arm") || getBuildConfiguration("arm64")
// skip on arm, arm64 due to bug 1511615
load(libdir + 'asserts.js');

function test() {
  // non object
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(0, "foo"); },
                         "expected _a CData object_ of a function pointer type, got the string \"foo\"");
  // non CData object
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(0, ["foo"]); },
                         "expected a _CData_ object of a function pointer type, got the array [\"foo\"]");

  // a CData which is not a pointer
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(0, ctypes.int32_t(0)); },
                         "expected a CData object of a function _pointer_ type, got ctypes.int32_t(0)");
  // a pointer CData which is not a function
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(0, ctypes.int32_t.ptr(0)); },
                         "expected a CData object of a _function_ pointer type, got ctypes.int32_t.ptr(ctypes.UInt64(\"0x0\"))");

  // null function
  let func_type = ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t,
                                      [ctypes.int32_t, ctypes.int32_t]).ptr;
  let f0 = func_type(0);
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(0, f0); },
                         "expected a CData object of a _non-NULL_ function pointer type, got ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t, [ctypes.int32_t, ctypes.int32_t]).ptr(ctypes.UInt64(\"0x0\"))");

  // a function with 2 arguments
  let f1 = func_type(x => x);
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(0, f1); },
                         "expected a function accepting exactly one argument, got ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t, [ctypes.int32_t, ctypes.int32_t])");

  // non CData in argument 1
  let func_type2 = ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t,
                                       [ctypes.int32_t.ptr]).ptr;
  let f2 = func_type2(x => x);
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(0, f2); },
                         "can't convert the number 0 to the type of argument 1 of ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t, [ctypes.int32_t.ptr]).ptr");

  // wrong struct in argument 1
  let test_struct = ctypes.StructType("test_struct", [{ "x": ctypes.int32_t }]);
  let func_type3 = ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t,
                                       [test_struct]).ptr;
  let f3 = func_type3(x => x);
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer({ "x": "foo" }, f3); },
                         "can't convert the string \"foo\" to the 'x' field (int32_t) of test_struct at argument 1 of ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t, [test_struct]).ptr");

  // different size in argument 1
  let func_type4 = ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t,
                                       [ctypes.int32_t]).ptr;
  let f4 = func_type4(x => x);
  assertTypeErrorMessage(() => { ctypes.CDataFinalizer(ctypes.int16_t(0), f4); },
                         "expected an object with the same size as argument 1 of ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [ctypes.int32_t]).ptr, got ctypes.int16_t(0)");

  let fin = ctypes.CDataFinalizer(ctypes.int32_t(0), f4);
  fin.dispose();
  assertTypeErrorMessage(() => { ctypes.int32_t(0).value = fin; },
                         "attempting to convert an empty CDataFinalizer");
  assertTypeErrorMessage(() => { f4(fin); },
                         /attempting to convert an empty CDataFinalizer at argument 1 of ctypes\.FunctionType\(ctypes\.default_abi, ctypes\.int32_t, \[ctypes\.int32_t\]\)\.ptr\(ctypes\.UInt64\(\"[x0-9A-Fa-f]+\"\)\)/);
}

if (typeof ctypes === "object")
  test();
