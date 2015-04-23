load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.PointerType({}); },
                         "argument of PointerType must be a CType");
}

if (typeof ctypes === "object")
  test();
