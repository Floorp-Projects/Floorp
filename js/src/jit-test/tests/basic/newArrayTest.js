function newArrayTest()
{
  var a = [];
  for (var i = 0; i < 10; i++)
    a[i] = new Array();
  return a.map(function(x) x.length).toString();
}
assertEq(newArrayTest(), "0,0,0,0,0,0,0,0,0,0");
