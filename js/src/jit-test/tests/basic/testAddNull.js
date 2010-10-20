function testAddNull()
{
  var rv;
  for (var x = 0; x < HOTLOOP + 1; ++x)
    rv = null + [,,];
  return rv;
}
assertEq(testAddNull(), "null,");
checkStats({
    recorderStarted: 1,
    sideExitIntoInterpreter: 1,
    recorderAborted: 0
});
