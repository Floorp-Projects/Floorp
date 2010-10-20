function setprop()
{
  var obj = { a:-1 };
  var obj2 = { b:-1, a:-1 };
  for (var i = 0; i < 20; i++) {
    obj2.b = obj.a = i;
  }
  return [obj.a, obj2.a, obj2.b].toString();
}
assertEq(setprop(), "19,-1,19");
