function testObjectOrderedCmp2()
{
  var a = new Array(5);
  for(var i=0;i<5;++i) a[i] = ("" <= null);
  return a.join(",");
}
assertEq(testObjectOrderedCmp2(), "true,true,true,true,true");
