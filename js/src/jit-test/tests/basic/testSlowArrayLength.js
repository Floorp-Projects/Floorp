function testSlowArrayLength()
{
  var counter = 0;
  var a = [];
  a[10000000 - 1] = 0;
  for (var i = 0; i < a.length; i++)
    counter++;
  return counter;
}
assertEq(testSlowArrayLength(), 10000000);
checkStats({
  recorderStarted: 1,
  recorderAborted: 0,
  sideExitIntoInterpreter: 1
});
