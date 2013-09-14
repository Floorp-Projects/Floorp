function testFloat32SetElemIC(a) {
  for (var i = 0; i < a.length; i++) {
    var r = Math.fround(Math.random());
    a[i] = r;
    assertEq(a[i], r);
  }
}

testFloat32SetElemIC(new Array(2048));
testFloat32SetElemIC(new Float64Array(2048));
testFloat32SetElemIC(new Float32Array(2048));
