// Don't die a float32-related death.
function testFloat32SetElemIC(a) {
  for (var i = 0; i < a.length; i++) {
    var r = Math.fround(Math.random());
    a[i] = r;
  }
}
testFloat32SetElemIC(new Array(2048));
testFloat32SetElemIC(new Uint8ClampedArray(2048));
