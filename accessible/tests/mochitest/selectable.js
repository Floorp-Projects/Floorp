/**
 * Test selection getter methods of nsIAccessibleSelectable.
 *
 * @param aIdentifier        [in] selectable container accessible
 * @param aSelectedChildren  [in] array of selected children
 */
function testSelectableSelection(aIdentifier, aSelectedChildren)
{
  var acc = getAccessible(aIdentifier, [nsIAccessibleSelectable]);
  if (!acc)
    return;

  var len = aSelectedChildren.length;

  // getSelectedChildren
  var selectedChildren = acc.GetSelectedChildren();
  is(selectedChildren ? selectedChildren.length : 0, aSelectedChildren.length,
     "getSelectedChildren: wrong selected children count for " + prettyName(aIdentifier));

  for (var idx = 0; idx < len; idx++) {
    var expectedAcc = getAccessible(aSelectedChildren[idx]);
    is(selectedChildren.queryElementAt(idx, nsIAccessible), expectedAcc,
       "getSelectedChildren: wrong selected child at index " + idx + " for " + prettyName(aIdentifier));
  }

  // selectionCount
  is(acc.selectionCount, aSelectedChildren.length,
     "selectionCount: wrong selected children count for " + prettyName(aIdentifier));

  // refSelection
  for (var idx = 0; idx < len; idx++) {
    var expectedAcc = getAccessible(aSelectedChildren[idx]);
    is(acc.refSelection(idx), expectedAcc,
       "refSelection: wrong selected child at index " + idx + " for " + prettyName(aIdentifier));
  }
}
