function testLengthInString()
{
  var s = new String();
  var res = "length" in s;
  for (var i = 0; i < 5; i++)
    res = res && ("length" in s);
  res = res && s.hasOwnProperty("length");
  for (var i = 0; i < 5; i++)
    res = res && s.hasOwnProperty("length");
  return res;
}
assertEq(testLengthInString(), true);
