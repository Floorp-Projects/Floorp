/**
 * Test table indexes.
 *
 * @param  aIdentifier  [in] table accessible identifier
 * @param  aLen         [in] cells count
 * @param  aRowIdxes    [in] array of row indexes for each cell index
 * @param  aColIdxes    [in] array of column indexes for each cell index
 */
function testTableIndexes(aIdentifier, aLen, aRowIdxes, aColIdxes)
{
  var tableAcc = getAccessible(aIdentifier, [nsIAccessibleTable]);
  if (!tableAcc)
    return;

  var row, column, index;
  var cellAcc;

  var id = prettyName(aIdentifier);

  for (var i = 0; i < aLen; i++) {
    try {
      row = tableAcc.getRowAtIndex(i);
    } catch (e) {
      ok(false, id + ": can't get row index for cell index " + i + ".");
    }

    try {
      column = tableAcc.getColumnAtIndex(i);
    } catch (e) {
      ok(false, id + ": can't get column index for cell index " + i + ".");
    }

    try {
      index = tableAcc.getIndexAt(aRowIdxes[i], aColIdxes[i]);
    } catch (e) {
      ok(false,
         id + ": can't get cell index by row index " + aRowIdxes[i] +
           " and column index: " + aColIdxes[i]  + ".");
    }

    is(row, aRowIdxes[i], id + ": row  for index " + i +" is nor correct");
    is(column, aColIdxes[i],
       id + ": column  for index " + i +" is not correct");
    is(index, i,
       id + ": row " + row + " /column " + column + " and index " + index + " aren't inconsistent.");

    try {
      cellAcc = null;
      cellAcc = tableAcc.cellRefAt(row, column);
    } catch (e) { }

    ok(cellAcc,
       id + ": Can't get cell accessible at row = " + row + ", column = " + column);

    if (cellAcc) {
      var attrs = cellAcc.attributes;
      var strIdx = "";
      try {
        strIdx = attrs.getStringProperty("table-cell-index");
      } catch (e) {
        ok(false,
           id + ": no cell index from object attributes on the cell accessible at index " + index + ".");
      }

      if (strIdx) {
        is (parseInt(strIdx), index,
            id + ": cell index from object attributes of cell accessible isn't corrent.");
      }
    }
  }
}
