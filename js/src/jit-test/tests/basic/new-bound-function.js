var funProto = Function.prototype;
assertEq(Object.getOwnPropertyDescriptor(funProto, "prototype"), undefined);

function Point(x, y) { this.x = x; this.y = y; }

var YAxisPoint = Point.bind(null, 0);

assertEq(YAxisPoint.prototype, undefined);

var oldPoint;
for (var i = 0, sz = 9; i < sz; oldPoint = point, i++)
{
  var point = new YAxisPoint(5);
  assertEq(point === oldPoint, false);
  assertEq(point.x, 0);
  assertEq(point.y, 5);
  assertEq(Object.getOwnPropertyDescriptor(funProto, "prototype"), undefined);
  assertEq(Object.getOwnPropertyDescriptor(YAxisPoint, "prototype"), undefined);
}

