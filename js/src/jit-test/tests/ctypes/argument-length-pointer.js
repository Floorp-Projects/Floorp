load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.PointerType(); },
                         "PointerType takes one argument");
  assertTypeErrorMessage(() => { ctypes.int32_t.ptr(1, 2, 3, 4); },
                         "PointerType constructor takes 0, 1, 2, or 3 arguments");
}

if (typeof ctypes === "object")
  test();
