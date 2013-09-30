if (!this.hasOwnProperty("TypedObject"))
  quit();

setJitCompilerOption("ion.usecount.trigger", 30);

var PointType2 =
  new TypedObject.StructType({
    x: TypedObject.float64,
    y: TypedObject.float64});

var PointType3 =
  new TypedObject.StructType({
    x: TypedObject.float64,
    y: TypedObject.float64,
    z: TypedObject.float64});

function xPlusY(p) {
  return p.x + p.y;
}

function xPlusYTweak(p) {
  p.x = 22;
  return xPlusY(p);
}

function foo() {
  var N = 100;
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
