load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.StructType(1); },
                         "first argument of StructType must be a string");
  assertTypeErrorMessage(() => { ctypes.StructType("a", 1); },
                         "second argument of StructType must be an array");
  assertTypeErrorMessage(() => { ctypes.StructType("a").define(1); },
                         "argument of StructType.prototype.define must be an array");
  assertTypeErrorMessage(() => { ctypes.StructType("a").define({}); },
                         "argument of StructType.prototype.define must be an array");
  assertTypeErrorMessage(() => { ctypes.StructType("a", [{x:ctypes.int32_t}])().addressOfField(1); },
                         "argument of StructType.prototype.addressOfField must be a string");
}

if (typeof ctypes === "object")
  test();
