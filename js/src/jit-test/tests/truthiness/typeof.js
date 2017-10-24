function t1(v)
{
  return typeof v;
}

assertEq(t1(createIsHTMLDDA()), "undefined");
assertEq(t1(createIsHTMLDDA()), "undefined");
assertEq(t1(createIsHTMLDDA()), "undefined");

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
assertEq(t2(createIsHTMLDDA()), "undefined");
assertEq(t2(createIsHTMLDDA()), "undefined");
assertEq(t2(createIsHTMLDDA()), "undefined");
