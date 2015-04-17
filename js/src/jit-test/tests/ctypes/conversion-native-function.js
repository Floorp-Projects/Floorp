// Type conversion error for native function should report its name and type
// in C style.

load(libdir + 'asserts.js');

function test() {
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
                         ctypes.double, ctypes.double);
  assertTypeErrorMessage(() => { func(1, "xyzzy"); },
                         "can't pass the string \"xyzzy\" to argument 2 of double hypot(double, double)");

  // test C style source for various types
  let test_struct = ctypes.StructType("test_struct", [{ "x": ctypes.int32_t }]);
  let test_func = ctypes.FunctionType(ctypes.default_abi, ctypes.voidptr_t,
                                      [ctypes.int32_t]).ptr;
  func = lib.declare("hypot",
                     ctypes.default_abi,
                     ctypes.double,
                     ctypes.double, ctypes.int32_t.ptr.ptr.ptr.array(),
                     test_struct, test_struct.ptr.ptr,
                     test_func, test_func.ptr.ptr.ptr, "...");
  assertTypeErrorMessage(() => { func("xyzzy", 1, 2, 3, 4, 5); },
                         "can't pass the string \"xyzzy\" to argument 1 of double hypot(double, int32_t****, struct test_struct, struct test_struct**, void* (*)(int32_t), void* (****)(int32_t), ...)");
}

if (typeof ctypes === "object")
  test();
