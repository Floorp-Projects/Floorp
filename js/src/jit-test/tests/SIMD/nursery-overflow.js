load(libdir + 'simd.js');

if (typeof SIMD === "undefined")
    quit();

setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);

var i4 = SIMD.int32x4;
var i4sub = SIMD.int32x4.sub;

function simdbox(i) {
  return i4(i, i, i, i);
}

function test() {
  var arr = [];

  // overflow the nursery with live SIMD objects.
  for (var i = 0; i < 100000; i++) {
    arr.push(simdbox(i));
  }

  return arr;
}

var arr = test();
for (var i = 0; i < arr.length; i++)
  assertEqX4(arr[i], [i, i, i, i]);
