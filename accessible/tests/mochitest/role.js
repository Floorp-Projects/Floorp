function testRole(aID, aRole)
{
  var acc = getAccessible(aID);
  if (!acc)
    return;

  try {
    is(acc.finalRole, aRole, "Wrong role for " + aID + "!");
  } catch(e) {
    ok(false, "Error getting role for " + aID + "!");
  }
}
