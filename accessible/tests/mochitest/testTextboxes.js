// Mapping needed state flags for easier handling.
const state_focusable = nsIAccessibleStates.STATE_FOCUSABLE;
const state_focused = nsIAccessibleStates.STATE_FOCUSED;
const state_readonly = nsIAccessibleStates.STATE_READONLY;

const ext_state_multi_line = nsIAccessibleStates.EXT_STATE_MULTI_LINE;
const ext_state_editable = nsIAccessibleStates.EXT_STATE_EDITABLE;
const ext_state_required = nsIAccessibleStates.STATE_REQUIRED;
const ext_state_invalid = nsIAccessibleStates.STATE_INVALID;

// Mapping needed final roles
const role_entry = nsIAccessibleRole.ROLE_ENTRY;
const role_password_text = nsIAccessibleRole.ROLE_PASSWORD_TEXT;

function testValue(aID, aAcc, aValue, aRole)
{
  if (aRole == role_password_text) {
    var value;
    try {
      value = aAcc.value;
      do_throw("We do not want a value on " + aID + "!");
    } catch(e) {
      is(e.result, Components.results.NS_ERROR_FAILURE,
         "Wrong return value for getValue on " + aID + "!");
    }
  } else
    is(aAcc.value, aValue, "Wrong value for " + aID + "!");
}

function testStates(aID, aAcc, aState, aExtraState, aAbsentState)
{
  var state = {}, extraState = {};
  aAcc.getState(state, extraState);
  is(state.value & aState, aState, "wrong state bits for " + aID + "!");
  is(extraState.value & aExtraState, aExtraState,
     "wrong extraState bits for " + aID + "!");
  if (aAbsentState != 0)
    is(state.value & aAbsentState, 0, "state bits should not be present in ID "
       + aID + "!");
  if (state.value & state_readonly)
    // if state is readonly, ext state must not be ext_state_editable.
    is(extraState.value & ext_state_editable, 0,
       "Read-only " + aID + " cannot be editable!");
}

function testAction(aID, aAcc, aNumActions, aActionName, aActionDescription)
{
  var numActions = aAcc.numActions;
  is(numActions, aNumActions, "Wrong number of actions for " + aID + "!");

  if (numActions != 0) {
    // Test first action. Normally only 1 should be present.
    is(aAcc.getActionName(0), aActionName,
       "Wrong name of action for " + aID + "!");
    is(aAcc.getActionDescription(0), aActionDescription,
       "Wrong description of action for " + aID + "!");
  }
}

function testThis(aID, aName, aValue, aDescription, aRole, aState,
                  aExtraState, aAbsentState, aNumActions, aActionName,
                  aActionDescription)
{
  var elem = document.getElementById(aID);
  var acc = null;
  try {
    acc = gAccRetrieval.getAccessibleFor(elem);
  } catch(e) {}
  ok(acc, "No accessible for " + aID + "!");

  if (acc) {
    is(acc.name, aName, "Wrong name for " + aID + "!");
    testValue(aID, acc, aValue, aRole);
    is(acc.description, aDescription, "Wrong description for " + aID + "!");
    is(acc.role, aRole, "Wrong role for " + aID + "!");

    testStates(aID, acc, aState, aExtraState, aAbsentState);

    testAction(aID, acc, aNumActions, aActionName, aActionDescription);
  }
}
