/**
 * Tests the states and extra states of the given accessible.
 * Also tests for unwanted states and extra states.
 * In addition, the function performs a few plausibility checks derived from the
 * sstates and extra states passed in.
 *
 * @param aAccOrElmOrID      The accessible, DOM element or ID to be tested.
 * @param aState             The state bits that are wanted.
 * @param aExtraState        The extra state bits that are wanted.
 * @param aAbsentState       State bits that are not wanted.
 * @param aAbsentExtraState  Extra state bits that are not wanted.
 */
function testStates(aAccOrElmOrID, aState, aExtraState, aAbsentState,
                    aAbsentExtraState)
{
  var [state, extraState] = getStates(aAccOrElmOrID);

  var id = prettyName(aAccOrElmOrID);

  is(state & aState, aState,
     "wrong state bits for " + id + "!");

  if (aExtraState)
    is(extraState & aExtraState, aExtraState,
       "wrong extra state bits for " + id + "!");

  if (aAbsentState)
    is(state & aAbsentState, 0,
       "state bits should not be present in ID " + id + "!");

  if (aAbsentExtraState)
    is(extraState & aAbsentExtraState, 0,
       "extraState bits should not be present in ID " + id + "!");

  if (state & STATE_READONLY)
    is(extraState & EXT_STATE_EDITABLE, 0,
       "Read-only " + id + " cannot be editable!");

  if (extraState & EXT_STATE_EDITABLE)
    is(state & STATE_READONLY, 0,
       "Editable " + id + " cannot be readonly!");

  if (state & STATE_COLLAPSED || state & STATE_EXPANDED)
    is(extraState & EXT_STATE_EXPANDABLE, EXT_STATE_EXPANDABLE,
       "Collapsed or expanded " + id + " should be expandable!");

  if (state & STATE_COLLAPSED)
    is(state & STATE_EXPANDED, 0,
       "Collapsed " + id + " cannot be expanded!");

  if (state & STATE_EXPANDED)
    is(state & STATE_COLLAPSED, 0,
       "Expanded " + id + " cannot be collapsed!");

  if (state & STATE_CHECKED || state & STATE_MIXED)
    is(state & STATE_CHECKABLE, STATE_CHECKABLE,
       "Checked or mixed " + id + " must be checkable!");

  if (state & STATE_CHECKED)
    is(state & STATE_MIXED, 0,
       "Checked " + id + " cannot be state mixed!");

  if (state & STATE_MIXED)
    is(state & STATE_CHECKED, 0,
       "Mixed " + id + " cannot be state checked!");

  if ((state & STATE_UNAVAILABLE) && !(state & STATE_INVISIBLE)
      && (getRole(aAccOrElmOrID) != ROLE_GROUPING))
    is(state & STATE_FOCUSABLE, STATE_FOCUSABLE,
       "Disabled " + id + " must be focusable!");
}

/**
 * Tests an acessible and its sub tree for the passed in state bits.
 * Used to make sure that states are propagated to descendants, for example the
 * STATE_UNAVAILABLE from a container to its children.
 *
 * @param aAccOrElmOrID  The accessible, DOM element or ID to be tested.
 * @param aState         The state bits that are wanted.
 * @param aExtraState    The extra state bits that are wanted.
 * @param aAbsentState   State bits that are not wanted.
 */
function testStatesInSubtree(aAccOrElmOrID, aState, aExtraState, aAbsentState)
{
  // test accessible and its subtree for propagated states.
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return;

  if (getRole(acc) != ROLE_TEXT_LEAF)
    // Right now, text leafs don't get tested because the states are not being
    // propagated.
    testStates(acc, aState, aExtraState, aAbsentState);

  // Iterate over its children to see if the state got propagated.
  var children = null;
  try {
    children = acc.children;
  } catch(e) {}
  ok(children, "Could not get children for " + aAccOrElmOrID +"!");

  if (children) {
    for (var i = 0; i < children.length; i++) {
      var childAcc = children.queryElementAt(i, nsIAccessible);
      testStatesInSubtree(childAcc, aState, aExtraState, aAbsentState);
    }
  }
}

function getStringStates(aAccOrElmOrID)
{
  var [state, extraState] = getStates(aAccOrElmOrID);
  return statesToString(state, extraState);
}

function getStates(aAccOrElmOrID)
{
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return [0, 0];
  
  var state = {}, extraState = {};
  acc.getState(state, extraState);

  return [state.value, extraState.value];
}
