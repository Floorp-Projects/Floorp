/**
 * Return the role of the given accessible. Return -1 if accessible could not
 * be retrieved.
 *
 * @param aAccOrElmOrID  [in] The accessible, DOM element or element ID the
 *                       accessible role is being requested for.
 */
function getRole(aAccOrElmOrID)
{
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return -1;

  var role = -1;
  try {
    role = acc.finalRole;
  } catch(e) {
    ok(false, "Role for " + aAccOrElmOrID + " could not be retrieved!");
  }

  return role;
}

/**
 * Test that the role of the given accessible is the role passed in.
 *
 * @param aAccOrElmOrID  the accessible, DOM element or ID to be tested.
 * @param aRole  The role that is to be expected.
 */
function testRole(aAccOrElmOrID, aRole)
{
  var role = getRole(aAccOrElmOrID);
  is(role, aRole, "Wrong role for " + aAccOrElmOrID + "!");
}
