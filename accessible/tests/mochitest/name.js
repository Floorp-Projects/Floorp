/**
 * Test accessible name for the given accessible identifier.
 */
function testName(aAccOrElmOrID, aName, aMsg)
{
  var msg = aMsg ? aMsg : "";

  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return;

  var txtID = prettyName(aAccOrElmOrID);
  try {
    is(acc.name, aName, msg + "Wrong name of the accessible for " + txtID);
  } catch (e) {
    ok(false, msg + "Can't get name of the accessible for " + txtID);
  }
  return acc;
}

/**
 * Test accessible description for the given accessible.
 */
function testDescr(aAccOrElmOrID, aDescr)
{
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
   return;

  is(acc.description, aDescr,
     "Wrong description for " + prettyName(aAccOrElmOrID));
}
