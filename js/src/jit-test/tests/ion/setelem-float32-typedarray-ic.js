function testFloat32SetElemIC(a) {
  for (var i = 0; i < a.length; i++) {
    var r = Math.fround(Math.random());
    a[i] = r;
    assertEq(a[i], r);
  }
}

function testCompoundClamping(a, b) {
  for (var i = 0; i < a.length; i++) {
    var r = Math.random();
    a[i] = b[i] = r;
    assertEq(a[i], b[i]);
  }
}

testFloat32SetElemIC(new Array(2048));
testFloat32SetElemIC(new Float64Array(2048));
testFloat32SetElemIC(new Float32Array(2048));

testCompoundClamping(new Array(2048), new Array(2048));
testCompoundClamping(new Uint8Array(2048), new Uint8Array(2048));
testCompoundClamping(new Uint8ClampedArray(2048), new Uint8ClampedArray(2048));
