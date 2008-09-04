function testName(aID, aName)
{
  var acc = getAccessible(aID);
  if (!acc) {
    ok(false, "No accessible for " + aID + "!");
  }

  is(acc.name, aName, "Wrong name of the accessible for " + aID);
  return acc;
}
