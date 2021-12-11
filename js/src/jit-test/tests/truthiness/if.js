function t1(v)
{
  if (v)
    return 1;
  return 0;
}

assertEq(t1(createIsHTMLDDA()), 0);
assertEq(t1(createIsHTMLDDA()), 0);
assertEq(t1(createIsHTMLDDA()), 0);

function t2(v)
{
  if (v)
    return 1;
  return 0;
}

assertEq(t2(17), 1);
assertEq(t2(0), 0);
assertEq(t2(-0), 0);
assertEq(t2(createIsHTMLDDA()), 0);
assertEq(t2(createIsHTMLDDA()), 0);
assertEq(t2(createIsHTMLDDA()), 0);
