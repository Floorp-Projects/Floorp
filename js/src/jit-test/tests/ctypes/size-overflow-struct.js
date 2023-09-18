load(libdir + 'asserts.js');

function test() {
  if (getBuildConfiguration("pointer-byte-size") == 4) {
    let big_array = ctypes.int32_t.array(0xfffffff);
    assertRangeErrorMessage(() => { ctypes.StructType("x", [{a: big_array},
                                                            {b: big_array},
                                                            {c: big_array},
                                                            {d: big_array},
                                                            {e: big_array}]); },
                            "struct size does not fit in size_t");
  } else if (getBuildConfiguration("pointer-byte-size") == 8) {
    let big_array = ctypes.int32_t.array(0xfffffffffffffff);
    assertRangeErrorMessage(() => { ctypes.StructType("x", [{a: big_array},
                                                            {b: big_array},
                                                            {c: big_array},
                                                            {d: big_array},
                                                            {e: big_array}]); },
                            "struct size does not fit in size_t");
  }
}

if (typeof ctypes === "object")
  test();
