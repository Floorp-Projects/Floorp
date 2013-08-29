var PointType2 = new StructType({x: float64,
                                 y: float64});

var PointType3 = new StructType({x: float64,
                                 y: float64,
                                 z: float64});

function xPlusY(p) {
  return p.x + p.y;
}

function foo() {
  var N = 30000;
  var points = [];
  for (var i = 0; i < N; i++) {
    var s;
    if ((i % 2) == 0 || true)
      s = xPlusY(new PointType2({x: i, y: i+1}));
    else
      s = xPlusY(new PointType3({x: i, y: i+1, z: i+2}));
    assertEq(s, i + i + 1);
  }
}

foo();
