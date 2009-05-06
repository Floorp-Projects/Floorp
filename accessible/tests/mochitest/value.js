////////////////////////////////////////////////////////////////////////////////
// Public methods

/**
 * Tests nsIAccessibleValue interface.
 *
 * @param aAccOrElmOrId  [in] identifier of accessible
 * @param aValue         [in] accessible value (nsIAccessible::value)
 * @param aCurrValue     [in] current value (nsIAccessibleValue::currentValue)
 * @param aMinValue      [in] minimum value (nsIAccessibleValue::minimumValue)
 * @param aMaxValue      [in] maximumn value (nsIAccessibleValue::maximumValue)
 * @param aMinIncr       [in] minimum increment value
 *                        (nsIAccessibleValue::minimumIncrement)
 */
function testValue(aAccOrElmOrId, aValue, aCurrValue,
                   aMinValue, aMaxValue, aMinIncr)
{
  var acc = getAccessible(aAccOrElmOrId, [nsIAccessibleValue]);
  if (!acc)
    return;

  is(acc.value, aValue, "Wrong value of " + prettyName(aAccOrElmOrId));

  is(acc.currentValue, aCurrValue,
     "Wrong current value of " + prettyName(aAccOrElmOrId));
  is(acc.minimumValue, aMinValue,
     "Wrong minimum value of " + prettyName(aAccOrElmOrId));
  is(acc.maximumValue, aMaxValue,
     "Wrong maximum value of " + prettyName(aAccOrElmOrId));
  is(acc.minimumIncrement, aMinIncr,
     "Wrong minimum increment value of " + prettyName(aAccOrElmOrId));
}
