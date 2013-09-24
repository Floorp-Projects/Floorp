if (!this.hasOwnProperty("Type"))
  quit();

var PointType2 = new StructType({x: float64,
                                 y: float64});

var PointType3 = new StructType({x: float64,
                                 y: float64,
                                 z: float64});

function xPlusY(p) {
  return p.x + p.y;
}

function xPlusYTweak(p) {
  p.x = 22;
  return xPlusY(p);
}

function foo() {
  var N = 30000;
  var points = [];
  var obj;
  var s;

  for (var i = 0; i < N; i++) {
    if ((i % 2) == 0 || true)
      obj = new PointType2({x: i, y: i+1});
    else
      obj = new PointType3({x: i, y: i+1, z: i+2});

    assertEq(xPlusY(obj), i + i + 1);
    assertEq(xPlusYTweak(obj), 22 + i + 1);
  }
}

foo();
