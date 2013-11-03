/**
 * Test selection getter methods of nsIAccessibleSelectable.
 *
 * @param aIdentifier        [in] selectable container accessible
 * @param aSelectedChildren  [in] array of selected children
 */
function testSelectableSelection(aIdentifier, aSelectedChildren, aMsg)
{
  var acc = getAccessible(aIdentifier, [nsIAccessibleSelectable]);
  if (!acc)
    return;

  var msg = aMsg ? aMsg : "";
  var len = aSelectedChildren.length;

  // getSelectedChildren
  var selectedChildren = acc.selectedItems;
  is(selectedChildren ? selectedChildren.length : 0, len,
     msg + "getSelectedChildren: wrong selected children count for " +
     prettyName(aIdentifier));

  for (var idx = 0; idx < len; idx++) {
    var expectedAcc = getAccessible(aSelectedChildren[idx]);
    var actualAcc = selectedChildren.queryElementAt(idx, nsIAccessible);
    is(actualAcc, expectedAcc,
       msg + "getSelectedChildren: wrong selected child at index " + idx +
       " for " + prettyName(aIdentifier) + " { actual : " +
       prettyName(actualAcc) + ", expected: " + prettyName(expectedAcc) + "}");
  }

  // selectedItemCount
  is(acc.selectedItemCount, aSelectedChildren.length,
     "selectedItemCount: wrong selected children count for " + prettyName(aIdentifier));

  // getSelectedItemAt
  for (var idx = 0; idx < len; idx++) {
    var expectedAcc = getAccessible(aSelectedChildren[idx]);
    is(acc.getSelectedItemAt(idx), expectedAcc,
       msg + "getSelectedItemAt: wrong selected child at index " + idx + " for " +
       prettyName(aIdentifier));
  }

  // isItemSelected
  testIsItemSelected(acc, acc, { value: 0 }, aSelectedChildren, msg);
}

/**
 * Test isItemSelected method, helper for testSelectableSelection
 */
function testIsItemSelected(aSelectAcc, aTraversedAcc, aIndexObj, aSelectedChildren, aMsg)
{
  var childCount = aTraversedAcc.childCount;
  for (var idx = 0; idx < childCount; idx++) {
    var child = aTraversedAcc.getChildAt(idx);
    var [state, extraState] = getStates(child);
    if (state & STATE_SELECTABLE) {
      var isSelected = false;
      var len = aSelectedChildren.length;
      for (var jdx = 0; jdx < len; jdx++) {
        if (child == getAccessible(aSelectedChildren[jdx])) {
          isSelected = true;
          break;
        }
      }

      // isItemSelected
      is(aSelectAcc.isItemSelected(aIndexObj.value++), isSelected,
         aMsg + "isItemSelected: wrong selected child " + prettyName(child) +
         " for " + prettyName(aSelectAcc));

      // selected state
      testStates(child, isSelected ? STATE_SELECTED : 0, 0,
                 !isSelected ? STATE_SELECTED : 0 , 0);

      continue;
    }

    testIsItemSelected(aSelectAcc, child, aIndexObj, aSelectedChildren);
  }
}
