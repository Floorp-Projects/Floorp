function test() {
  for (let i = 0; i < 100; i++) {
    let test_struct = ctypes.StructType("test_struct", [{ "x": ctypes.int32_t },
                                                        { "bar": ctypes.uint32_t }]);

    try {
      new test_struct("foo", "x");
    } catch (e) {
    }
  }
}

if (typeof ctypes === "object")
  test();
