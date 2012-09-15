function t1(v)
{
  return typeof v;
}

assertEq(t1(objectEmulatingUndefined()), "undefined");
assertEq(t1(objectEmulatingUndefined()), "undefined");
assertEq(t1(objectEmulatingUndefined()), "undefined");

function t2(v)
{
  return typeof v;
}

assertEq(t2(17), "number");
assertEq(t2(0), "number");
assertEq(t2(-0), "number");
assertEq(t2(function(){}), "function");
assertEq(t2({}), "object");
assertEq(t2(null), "object");
assertEq(t2(objectEmulatingUndefined()), "undefined");
assertEq(t2(objectEmulatingUndefined()), "undefined");
assertEq(t2(objectEmulatingUndefined()), "undefined");
