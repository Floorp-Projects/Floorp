function stringSplitTest()
{
  var s = "a,b"
  var a = null;
  for (var i = 0; i < 10; ++i)
    a = s.split(",");
  return a.join();
}
assertEq(stringSplitTest(), "a,b");
