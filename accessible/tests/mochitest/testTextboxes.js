function testValue(aID, aAcc, aValue, aRole)
{
  is(aAcc.value, aValue, "Wrong value for " + aID + "!");
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

function testThis(aID, aName, aValue, aDescription, aRole,
                  aNumActions, aActionName, aActionDescription)
{
  var acc = getAccessible(aID);
  if (!acc)
    return;

  is(acc.name, aName, "Wrong name for " + aID + "!");
  testValue(aID, acc, aValue, aRole);
  is(acc.description, aDescription, "Wrong description for " + aID + "!");
  testRole(aID, aRole);

  testAction(aID, acc, aNumActions, aActionName, aActionDescription);
}
