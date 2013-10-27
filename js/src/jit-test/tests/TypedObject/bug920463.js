if (!this.hasOwnProperty("TypedObject"))
  throw new TypeError();

var StructType = TypedObject.StructType;
var float64 = TypedObject.float64;

var PointType3 = new StructType({ x: float64, y: float64});
function xPlusY(p) {
  return p.x + p.y;
}
var N = 30000;
for (var i = 0; i < N; ++i && xPlusY(function () { p; }) ) {
  obj = new PointType3();
  xPlusY(obj)
}
