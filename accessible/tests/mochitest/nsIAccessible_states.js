function testStates(aAccOrElmOrID, aState, aExtraState, aAbsentState)
{
  var [state, extraState] = getStates(aAccOrElmOrID);

  is(state & aState, aState,
     "wrong state bits for " + aAccOrElmOrID + "!");
  is(extraState & aExtraState, aExtraState,
     "wrong extraState bits for " + aAccOrElmOrID + "!");

  if (aAbsentState)
    is(state & aAbsentState, 0,
       "state bits should not be present in ID " + aAccOrElmOrID + "!");

  if (state & STATE_READONLY)
    // if state is readonly, ext state must not be ext_state_editable.
    is(extraState & EXT_STATE_EDITABLE, 0,
       "Read-only " + aAccOrElmOrID + " cannot be editable!");
}

function getStringStates(aAccOrElmOrID)
{
  var [state, extraState] = getStates(aAccOrElmOrID);
  var list = gAccRetrieval.getStringStates(state, extraState);

  var str = "";
  for (var index = 0; index < list.length; index++)
    str += list.item(index) + ", ";

  return str;
}

function getStates(aAccOrElmOrID)
{
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return [0, 0];
  
  var state = {}, extraState = {};
  acc.getFinalState(state, extraState);

  return [state.value, extraState.value];
}
