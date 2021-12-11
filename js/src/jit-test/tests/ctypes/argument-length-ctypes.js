load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.cast(); },
                         "ctypes.cast takes two arguments");
  assertTypeErrorMessage(() => { ctypes.getRuntime(); },
                         "ctypes.getRuntime takes one argument");
}

if (typeof ctypes === "object")
  test();
