/**
 * Test accessible name for the given accessible identifier.
 */
function testName(aAccOrElmOrID, aName, aMsg, aTodo)
{
  var msg = aMsg ? aMsg : "";

  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return;

  var func = aTodo ? todo_is : is;
  var txtID = prettyName(aAccOrElmOrID);
  try {
    func(acc.name, aName, msg + "Wrong name of the accessible for " + txtID);
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
