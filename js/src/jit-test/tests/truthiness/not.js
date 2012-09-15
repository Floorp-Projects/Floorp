function t1(v)
{
  if (!v)
    return 1;
  return 0;
}

assertEq(t1(objectEmulatingUndefined()), 1);
assertEq(t1(objectEmulatingUndefined()), 1);
assertEq(t1(objectEmulatingUndefined()), 1);

function t2(v)
{
  if (!v)
    return 1;
  return 0;
}

assertEq(t2(17), 0);
assertEq(t2(0), 1);
assertEq(t2(-0), 1);
assertEq(t2(objectEmulatingUndefined()), 1);
assertEq(t2(objectEmulatingUndefined()), 1);
assertEq(t2(objectEmulatingUndefined()), 1);
