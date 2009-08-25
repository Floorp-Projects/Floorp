function testNewArrayCount()
{
  var a = [];
  for (var i = 0; i < 5; i++)
    a = [0];
  assertEq(a.__count__, 1);
  for (var i = 0; i < 5; i++)
    a = [0, , 2];
  assertEq(a.__count__, 2);
}
testNewArrayCount();
