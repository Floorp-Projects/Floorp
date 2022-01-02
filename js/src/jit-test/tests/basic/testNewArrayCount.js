function testNewArrayCount()
{
  function count(a) { var n = 0; for (var p in a) n++; return n; }
  var a = [];
  for (var i = 0; i < 5; i++)
    a = [0];
  assertEq(count(a), 1);
  for (var i = 0; i < 5; i++)
    a = [0, , 2];
  assertEq(count(a), 2);
}
testNewArrayCount();
