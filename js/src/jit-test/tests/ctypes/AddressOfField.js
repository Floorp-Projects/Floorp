load(libdir + 'asserts.js');

function test() {
  let strcut = ctypes.StructType("a", [ { "x": ctypes.int32_t, } ])();
  for (let arg of [1, undefined, null, false, {}, [], Symbol("foo")]) {
    assertThrowsInstanceOf(() => { struct.addressOfField(arg); },
                           Error);
  }
}

if (typeof ctypes === "object")
  test();
