// Arrays should be initialized to zero

function f() {
  for (var ctor of [ Int8Array,
                     Uint8Array,
                     Uint8ClampedArray,
                     Int16Array,
                     Uint16Array,
                     Int32Array,
                     Uint32Array,
                     Float32Array,
                     Float64Array ])
  {
    for (var len of [ 3, 30, 300, 3000, 30000 ]) {
      var arr = new ctor(len);
      for (var i = 0; i < arr.length; i++)
        assertEq(arr[i], 0, "index " + i + " of " + ctor.name + " len " + len);
    }
  }
}

f();
f();
gc()
f();
