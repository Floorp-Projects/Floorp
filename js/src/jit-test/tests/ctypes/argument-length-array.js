load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.ArrayType(); },
                         "ArrayType takes one or two arguments");
  assertTypeErrorMessage(() => { ctypes.int32_t.array(10)(1, 2); },
                         "size defined ArrayType constructor takes at most one argument");
  assertTypeErrorMessage(() => { ctypes.int32_t.array()(1, 2); },
                         "size undefined ArrayType constructor takes one argument");
  assertTypeErrorMessage(() => { ctypes.int32_t.array(10)().addressOfElement(); },
                         "ArrayType.prototype.addressOfElement takes one argument");
}

if (typeof ctypes === "object")
  test();
