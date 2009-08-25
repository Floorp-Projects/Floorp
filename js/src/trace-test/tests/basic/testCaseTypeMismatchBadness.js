function testCaseTypeMismatchBadness()
{
  for (var z = 0; z < 3; ++z)
  {
    switch ("")
    {
      default:
      case 9:
        break;

      case "":
      case <x/>:
        break;
    }
  }

  return "no crash";
}
assertEq(testCaseTypeMismatchBadness(), "no crash");
checkStats({
    recorderAborted: 0
});
