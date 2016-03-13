load(libdir + 'asserts.js');

function test() {
  assertRangeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, []).ptr(x=>1)(1); },
                         "number of arguments does not match declaration of ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t) (expected 0, got 1)");

  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [1]); },
                         "the type of argument 1 is not a ctypes type (got the number 1)");
  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [ctypes.void_t]); },
                         "the type of argument 1 cannot be void or function (got ctypes.void)");
  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [])]); },
                         "the type of argument 1 cannot be void or function (got ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t))");
  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [ctypes.StructType("a")]); },
                         "the type of argument 1 must have defined size (got ctypes.StructType(\"a\"))");

  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [])(); },
                         "cannot construct from FunctionType; use FunctionType.ptr instead");

  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, 1, []); },
                         "return type is not a ctypes type (got the number 1)");
  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t.array(), []); },
                         "return type cannot be an array or function (got ctypes.int32_t.array())");
  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, []), []); },
                         "return type cannot be an array or function (got ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t))");
  assertTypeErrorMessage(() => { ctypes.FunctionType(ctypes.default_abi, ctypes.StructType("a"), []); },
                         "return type must have defined size (got ctypes.StructType(\"a\"))");

  let lib;
  try {
    lib = ctypes.open(ctypes.libraryName("c"));
  } catch (e) {
  }
  if (!lib)
    return;

  let func = lib.declare("hypot",
                         ctypes.default_abi,
                         ctypes.double,
                         ctypes.double, "...");
  assertRangeErrorMessage(() => { func(); },
                          "number of arguments does not match declaration of double hypot(double, ...) (expected 1 or more, got 0)");
  assertTypeErrorMessage(() => { func(1, 2); },
                          "variadic argument 2 must be a CData object (got the number 2)");
}

if (typeof ctypes === "object")
  test();
