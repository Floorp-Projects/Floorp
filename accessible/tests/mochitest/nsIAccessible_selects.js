/**
 * Tests an accessible and its children.
 * The accessible is one of the following:
 * - HTML:select (ROLE_COMBOBOX)
 * - HTML:select @size (ROLE_LIST)
 * - HTML:label containing either of the above (ROLE_LABEL)
 *
 * @param aID               The ID of the base element to test.
 * @param aNames            The array of expected names for the accessible and
 *                          its children.
 * @param aRoles            The array of expected roles for the accessible and
 *                          its children.
 * @param aStates           The array of states to test for on each accessible.
 * @param aUndesiredStates  Array of states we don't want to have for each
 *                          accessible.
 *
 * @note Each of the three arrays must have an equal number of elements. The
 * order of elements corresponds to the order in which the tree is walked from
 * the base accessible downwards.
 */
function testSelect(aID, aNames, aRoles, aStates, aUndesiredStates)
{
  // used to walk the tree starting at the aID's accessible.
  var acc = getAccessible(aID);
  if (!acc) {
    return;
  }

  testThis(aID, acc, aNames, aRoles, aStates, aUndesiredStates, 0);
}

/**
 * Tests a single accessible.
 * Recursively calls itself until it reaches the last option item.
 * Called first time from the testSelect function.
 *
 * @param aID               @see testSelect
 * @param aAcc              The accessible to test.
 * @param aNames            @see testSelect
 * @param aRoles            @see testSelect
 * @param aStates           @see testSelect
 * @param aUndesiredStates  @see testSelect
 * @param aIndex            The index in the three arrays to use. Used for both
 *                          the error message and for walking the tree downwards
 *                          from the base accessible.
 */
function testThis(aID, aAcc, aNames, aRoles, aStates, aUndesiredStates, aIndex)
{
  if (aIndex >= aNames.length)
    return;  // End of test
  else if (!aAcc) {
    ok(false, "No accessible for " + aID + " at index " + aIndex + "!");
    return;
  }

  is(aAcc.name, aNames[aIndex],
     "wrong name for " + aID + " at index " + aIndex + "!");
  var role = aAcc.role;
  is(role, aRoles[aIndex],
     "Wrong role for " + aID + " at index " + aIndex + "!");
  testStates(aID, aAcc, aStates, aUndesiredStates, aIndex);
  switch(role) {
    case ROLE_COMBOBOX:
    case ROLE_COMBOBOX_LIST:
    case ROLE_LABEL:
    case ROLE_LIST:
      // All of these expect the next item to be the first child of the current
      // accessible.
      var acc = null;
      try {
        acc = aAcc.firstChild;
      } catch(e) {}
      testThis(aID, acc, aNames, aRoles, aStates, aUndesiredStates, ++aIndex);
      break;
    case ROLE_COMBOBOX_OPTION:
    case ROLE_OPTION:
    case ROLE_TEXT_LEAF:
      // All of these expect the next item's accessible to be the next sibling.
      var acc = null;
      try {
        acc = aAcc.nextSibling;
      } catch(e) {}
      testThis(aID, acc, aNames, aRoles, aStates, aUndesiredStates, ++aIndex);
      break;
    default:
      break;
  }
}

/**
 * Tests the states for the given accessible.
 * Does not test for extraStates since we don't need these.
 *
 * @param aID  the ID of the base element.
 * @param aAcc  the current accessible from the recursive algorithm.
 * @param aStates  the states array passed down from the testThis function.
 * @param aUndesiredStates  the array of states we don't want to see, if any.
 * @param aIndex  the index of the item to test, determined and passed in from
 *                the testThis function.
 */
function testStates(aID, aAcc, aStates, aUndesiredStates, aIndex)
{
  var state = {}, extraState = {};
  aAcc.getState(state, extraState);
  if (aStates[aIndex] != 0)
    is(state.value & aStates[aIndex], aStates[aIndex],
       "Wrong state bits for " + aID + " at index " + aIndex + "!");
  if (aUndesiredStates[aIndex] != 0)
    is(state.value & aUndesiredStates[aIndex], 0,
       "Wrong undesired state bits for " + aID + " at index " + aIndex + "!");
}
