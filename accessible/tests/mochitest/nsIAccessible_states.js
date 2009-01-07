function testStates(aAccOrElmOrID, aState, aExtraState, aAbsentState,
                    aAbsentExtraState)
{
  var [state, extraState] = getStates(aAccOrElmOrID);

  is(state & aState, aState,
     "wrong state bits for " + aAccOrElmOrID + "!");

  if (aExtraState)
    is(extraState & aExtraState, aExtraState,
       "wrong extra state bits for " + aAccOrElmOrID + "!");

  if (aAbsentState)
    is(state & aAbsentState, 0,
       "state bits should not be present in ID " + aAccOrElmOrID + "!");

  if (aAbsentExtraState)
    is(extraState & aAbsentExtraState, 0,
       "extraState bits should not be present in ID " + aAccOrElmOrID + "!");

  if (state & STATE_READONLY)
    is(extraState & EXT_STATE_EDITABLE, 0,
       "Read-only " + aAccOrElmOrID + " cannot be editable!");

  if (extraState & EXT_STATE_EDITABLE)
    is(state & STATE_READONLY, 0,
       "Editable " + aAccOrElmOrID + " cannot be readonly!");

  if (state & STATE_COLLAPSED || state & STATE_EXPANDED)
    is(extraState & EXT_STATE_EXPANDABLE, EXT_STATE_EXPANDABLE,
       "Collapsed or expanded " + aAccOrElmOrID + " should be expandable!");

  if (state & STATE_COLLAPSED)
    is(state & STATE_EXPANDED, 0,
       "Collapsed " + aAccOrElmOrID + " cannot be expanded!");

  if (state & STATE_EXPANDED)
    is(state & STATE_COLLAPSED, 0,
       "Expanded " + aAccOrElmOrID + " cannot be collapsed!");
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
  acc.getState(state, extraState);

  return [state.value, extraState.value];
}
