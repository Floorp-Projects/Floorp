var funProto = Function.prototype;
assertEq(Object.getOwnPropertyDescriptor(funProto, "prototype"), undefined);
assertEq(parseInt.prototype, undefined);
var oldObj;
for (var i = 0, sz = 9; i < sz; oldObj = obj, i++)
{

  try {
    var obj = new funProto;
  }
  catch (e) {}
  assertEq(Object.getOwnPropertyDescriptor(funProto, "prototype"), undefined);
  assertEq(Object.getOwnPropertyDescriptor(parseInt, "prototype"), undefined);
  assertEq(parseInt.prototype, undefined);
}

