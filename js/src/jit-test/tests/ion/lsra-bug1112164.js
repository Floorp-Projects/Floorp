setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);

var atan2 = Math.atan2;

function reference(x, y, z, w) {
  with({}) {}; /* prevent compilation */
  return [ atan2(x + 0.1, w),
           atan2(y + 0.1, z),
           atan2(z + 0.1, y),
           atan2(w + 0.1, x) ];
}

function generator(x, y, z, w) {
  return [ atan2(x + 0.1, w),
           atan2(y + 0.1, z),
           atan2(z + 0.1, y),
           atan2(w + 0.1, x) ];
}

function test() {
  var min = -0.99999, step = 0.1, max = 1;
  for (var x = min; x < max; x += step)
    for (var y = min; y < max; y += step)
      for (var z = min; z < max; z += step)
        for (var w = min; w < max; w += step) {
          var ref = reference(x, y, z, w);
          var gen = generator(x, y, z, w);
          assertEq(gen[0], ref[0]);
          assertEq(gen[1], ref[1]);
          assertEq(gen[2], ref[2]);
          assertEq(gen[3], ref[3]);
        }
}

test();
