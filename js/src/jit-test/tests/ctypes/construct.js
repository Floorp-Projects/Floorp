load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.void_t(); },
                         "cannot construct from void_t");
  assertTypeErrorMessage(() => { ctypes.CType(); },
                         "cannot construct from abstract type");
}

if (typeof ctypes === "object")
  test();
