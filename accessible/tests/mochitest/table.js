/**
 * This file provides set of helper functions to test nsIAccessibleTable
 * interface.
 *
 * Required:
 *   common.js
 *   states.js
 */

/**
 * Test table indexes.
 *
 * @param  aIdentifier  [in] table accessible identifier
 * @param  aIdxes       [in] two dimensional array of cell indexes
 */
function testTableIndexes(aIdentifier, aIdxes)
{
  var tableAcc = getAccessible(aIdentifier, [nsIAccessibleTable]);
  if (!tableAcc)
    return;

  var obtainedRowIdx, obtainedColIdx, obtainedIdx;
  var cellAcc;

  var id = prettyName(aIdentifier);

  var rowCount = aIdxes.length;
  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    var colCount = aIdxes[rowIdx].length;
    for (var colIdx = 0; colIdx < colCount; colIdx++) {
      var idx = aIdxes[rowIdx][colIdx];

      // cellRefAt
      try {
        cellAcc = null;
        cellAcc = tableAcc.cellRefAt(rowIdx, colIdx);
      } catch (e) { }
      
      ok(idx != -1 && cellAcc || idx == -1 && !cellAcc,
         id + ": Can't get cell accessible at row = " + rowIdx + ", column = " + colIdx);

      if (idx != - 1) {
        // getRowAtIndex
        var origRowIdx = rowIdx;
        while (origRowIdx > 0 &&
               aIdxes[rowIdx][colIdx] == aIdxes[origRowIdx - 1][colIdx])
          origRowIdx--;

        try {
          obtainedRowIdx = tableAcc.getRowAtIndex(idx);
        } catch (e) {
          ok(false, id + ": can't get row index for cell index " + idx + "," + e);
        }

        is(obtainedRowIdx, origRowIdx,
           id + ": row  for index " + idx +" is not correct");

        // getColumnAtIndex
        var origColIdx = colIdx;
        while (origColIdx > 0 &&
               aIdxes[rowIdx][colIdx] == aIdxes[rowIdx][origColIdx - 1])
          origColIdx--;

        try {
          obtainedColIdx = tableAcc.getColumnAtIndex(idx);
        } catch (e) {
          ok(false, id + ": can't get column index for cell index " + idx + "," + e);
        }

        is(obtainedColIdx, origColIdx,
           id + ": column  for index " + idx +" is not correct");

        // 'table-cell-index' attribute
        if (cellAcc) {
          var attrs = cellAcc.attributes;
          var strIdx = "";
          try {
            strIdx = attrs.getStringProperty("table-cell-index");
          } catch (e) {
            ok(false,
               id + ": no cell index from object attributes on the cell accessible at index " + idx + ".");
          }

          if (strIdx) {
            is (parseInt(strIdx), idx,
                id + ": cell index from object attributes of cell accessible isn't corrent.");
          }
        }
      }

      // getIndexAt
      try {
        obtainedIdx = tableAcc.getIndexAt(rowIdx, colIdx);
      } catch (e) {
        obtainedIdx = -1;
      }

      is(obtainedIdx, idx,
         id + ": row " + rowIdx + " /column " + colIdx + " and index " + obtainedIdx + " aren't inconsistent.");
    }
  }
}

/**
 * Constants used to describe cells states array. Values 0 and 1 are reserved
 * for boolean flag indicating the cell is origin and it is whether unselected
 * or selected.
 */
const kRowSpanned = 2; // Indicates the cell is not origin and row spanned 
const kColSpanned = 4; // Indicates the cell is not origin and column spanned
const kSpanned = kRowSpanned | kColSpanned;

/**
 * Test table getters selection methods.
 *
 * @param  aIdentifier  [in] table accessible identifier
 * @param  aCellsArray  [in] two dimensional array (row X columns) of cells
 *                       states (either boolean (selected/unselected) if cell is
 *                       origin, or one of constants defined above).
 * @param  aMsg         [in] text appended before every message
 */
function testTableSelection(aIdentifier, aCellsArray, aMsg)
{
  var msg = aMsg ? aMsg : "";
  var acc = getAccessible(aIdentifier, [nsIAccessibleTable]);
  if (!acc)
    return;

  var rowsCount = aCellsArray.length;
  var colsCount = aCellsArray[0].length;

  // Columns selection tests.
  var selCols = new Array();

  // isColumnSelected test
  for (var colIdx = 0; colIdx < colsCount; colIdx++) {
    var isColSelected = true;
    for (var rowIdx = 0; rowIdx < rowsCount; rowIdx++) {
      if (aCellsArray[rowIdx][colIdx] == false) {
        isColSelected = false;
        break;
      }
    }

    is(acc.isColumnSelected(colIdx), isColSelected,
       msg + "Wrong selection state of " + colIdx + " column for " +
       prettyName(aIdentifier));

    if (isColSelected)
      selCols.push(colIdx);
  }

  // selectedColsCount test
  is(acc.selectedColumnsCount, selCols.length,
     msg + "Wrong count of selected columns for " + prettyName(aIdentifier));

  // getSelectedColumns test
  var actualSelColsCountObj = { value: null };
  var actualSelCols = acc.getSelectedColumns(actualSelColsCountObj);

  var actualSelColsCount = actualSelColsCountObj.value;
  is (actualSelColsCount, selCols.length,
      msg + "Wrong count of selected columns for " + prettyName(aIdentifier) +
      "from getSelectedColumns.");

  for (var i = 0; i < actualSelColsCount; i++) {
    is (actualSelCols[i], selCols[i],
        msg + "Column at index " + selCols[i] + " should be selected.");
  }

  // Rows selection tests.
  var selRows = new Array();

  // isRowSelected test
  var selRowsCount = 0;
  for (var rowIdx = 0; rowIdx < rowsCount; rowIdx++) {
    var isRowSelected = true;
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      if (aCellsArray[rowIdx][colIdx] == false) {
        isRowSelected = false;
        break;
      }
    }

    is(acc.isRowSelected(rowIdx), isRowSelected,
       msg + "Wrong selection state of " + rowIdx + " row for " +
       prettyName(aIdentifier));

    if (isRowSelected)
      selRows.push(rowIdx);
  }

  // selectedRowsCount test
  is(acc.selectedRowsCount, selRows.length,
     msg + "Wrong count of selected rows for " + prettyName(aIdentifier));

  // getSelectedRows test
  var actualSelRowsCountObj = { value: null };
  var actualSelRows = acc.getSelectedRows(actualSelRowsCountObj);

  var actualSelRowsCount = actualSelRowsCountObj.value;
  is (actualSelRowsCount, selRows.length,
      msg + "Wrong count of selected rows for " + prettyName(aIdentifier) +
      "from getSelectedRows.");

  for (var i = 0; i < actualSelRowsCount; i++) {
    is (actualSelRows[i], selRows[i],
        msg + "Row at index " + selRows[i] + " should be selected.");
  }

  // Cells selection tests.
  var selCells = new Array();

  // isCellSelected test
  for (var rowIdx = 0; rowIdx < rowsCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      if (!isOrigCell(aCellsArray[rowIdx][colIdx]))
        continue;

      is(acc.isCellSelected(rowIdx, colIdx), aCellsArray[rowIdx][colIdx],
         msg + "Wrong selection state of cell at " + rowIdx + " row and " +
         colIdx + " column for " + prettyName(aIdentifier));

      if (aCellsArray[rowIdx][colIdx])
        selCells.push(acc.getIndexAt(rowIdx, colIdx));
    }
  }

  // selectedCellsCount tests
  is(acc.selectedCellsCount, selCells.length,
     msg + "Wrong count of selected cells for " + prettyName(aIdentifier));

  // getSelectedCells test
  var actualSelCellsCountObj = { value: null };
  var actualSelCells = acc.getSelectedCells(actualSelCellsCountObj);

  var actualSelCellsCount = actualSelCellsCountObj.value;
  is (actualSelCellsCount, selCells.length,
      msg + "Wrong count of selected cells for " + prettyName(aIdentifier) +
      "from getSelectedCells.");

  for (var i = 0; i < actualSelCellsCount; i++) {
    is (actualSelCells[i], selCells[i],
        msg + "Cell at index " + selCells[i] + " should be selected.");
  }

  // selected states tests
  for (var rowIdx = 0; rowIdx < rowsCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      if (!isOrigCell(aCellsArray[rowIdx][colIdx]))
        continue;

      var cell = acc.cellRefAt(rowIdx, colIdx);
      var isSel = aCellsArray[rowIdx][colIdx];
      if (isSel)
        testStates(cell, STATE_SELECTED);
      else
        testStates(cell, STATE_SELECTABLE, 0, STATE_SELECTED);
    }
  }
}

/**
 * Test unselectColumn method of accessible table.
 */
function testUnselectTableColumn(aIdentifier, aColIdx, aCellsArray)
{
  var acc = getAccessible(aIdentifier, [nsIAccessibleTable]);
  if (!acc)
    return;

  var rowsCount = aCellsArray.length;
  for (var rowIdx = 0; rowIdx < rowsCount; rowIdx++) {
    var cellState = aCellsArray[rowIdx][aColIdx];
    // Unselect origin cell.
    var [origRowIdx, origColIdx] =
      getOrigRowAndColumn(aCellsArray, rowIdx, aColIdx);
    aCellsArray[origRowIdx][origColIdx] = false;
  }

  acc.unselectColumn(aColIdx);
  testTableSelection(aIdentifier, aCellsArray,
                     "Unselect " + aColIdx + " column: ");
}

/**
 * Test selectColumn method of accessible table.
 */
function testSelectTableColumn(aIdentifier, aColIdx, aCellsArray)
{
  var acc = getAccessible(aIdentifier, [nsIAccessibleTable]);
  if (!acc)
    return;

  var rowsCount = aCellsArray.length;
  var colsCount = aCellsArray[0].length;

  for (var rowIdx = 0; rowIdx < rowsCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      var cellState = aCellsArray[rowIdx][colIdx];

      if (colIdx == aColIdx) { // select target column
        if (isOrigCell(cellState)) {
          // Select the cell if it is origin.
          aCellsArray[rowIdx][colIdx] = true;

        } else {
          // If the cell is spanned then search origin cell and select it.
          var [origRowIdx, origColIdx] = getOrigRowAndColumn(aCellsArray,
                                                             rowIdx, colIdx);
          aCellsArray[origRowIdx][origColIdx] = true;
        }

      } else if (isOrigCell(cellState)) { // unselect other columns
        if (colIdx > aColIdx) {
          // Unselect the cell if traversed column index is greater than column
          // index of target cell.
          aCellsArray[rowIdx][colIdx] = false;

        } else if (!isColSpannedCell(aCellsArray[rowIdx][aColIdx])) {
          // Unselect the cell if the target cell is not row spanned.
          aCellsArray[rowIdx][colIdx] = false;

        } else {
          // Unselect the cell if it is not spanned to the target cell.
          for (var spannedColIdx = colIdx + 1; spannedColIdx < aColIdx;
               spannedColIdx++) {
            var spannedCellState = aCellsArray[rowIdx][spannedColIdx];
            if (!isRowSpannedCell(spannedCellState)) {
              aCellsArray[rowIdx][colIdx] = false;
              break;
            }
          }
        }
      }
    }
  }

  acc.selectColumn(aColIdx);
  testTableSelection(aIdentifier, aCellsArray,
                     "Select " + aColIdx + " column: ");
}

/**
 * Test unselectRow method of accessible table.
 */
function testUnselectTableRow(aIdentifier, aRowIdx, aCellsArray)
{
  var acc = getAccessible(aIdentifier, [nsIAccessibleTable]);
  if (!acc)
    return;

  var colsCount = aCellsArray[0].length;
  for (var colIdx = 0; colIdx < colsCount; colIdx++) {
    // Unselect origin cell.
    var [origRowIdx, origColIdx] = getOrigRowAndColumn(aCellsArray,
                                                       aRowIdx, colIdx);
    aCellsArray[origRowIdx][origColIdx] = false;
  }

  acc.unselectRow(aRowIdx);
  testTableSelection(aIdentifier, aCellsArray,
                     "Unselect " + aRowIdx + " row: ");
}

/**
 * Test selectRow method of accessible table.
 */
function testSelectTableRow(aIdentifier, aRowIdx, aCellsArray)
{
  var acc = getAccessible(aIdentifier, [nsIAccessibleTable]);
  if (!acc)
    return;

  var rowsCount = aCellsArray.length;
  var colsCount = aCellsArray[0].length;

  for (var rowIdx = 0; rowIdx < rowsCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      var cellState = aCellsArray[rowIdx][colIdx];

      if (rowIdx == aRowIdx) { // select the given row
        if (isOrigCell(cellState)) {
          // Select the cell if it is origin.
          aCellsArray[rowIdx][colIdx] = true;

        } else {
          // If the cell is spanned then search origin cell and select it.
          var [origRowIdx, origColIdx] = getOrigRowAndColumn(aCellsArray,
                                                             rowIdx, colIdx);

          aCellsArray[origRowIdx][origColIdx] = true;
        }

      } else if (isOrigCell(cellState)) { // unselect other rows
        if (rowIdx > aRowIdx) {
          // Unselect the cell if traversed row index is greater than row
          // index of target cell.
          aCellsArray[rowIdx][colIdx] = false;

        } else if (!isRowSpannedCell(aCellsArray[aRowIdx][colIdx])) {
          // Unselect the cell if the target cell is not row spanned.
          aCellsArray[rowIdx][colIdx] = false;

        } else {
          // Unselect the cell if it is not spanned to the target cell.
          for (var spannedRowIdx = rowIdx + 1; spannedRowIdx < aRowIdx;
               spannedRowIdx++) {
            var spannedCellState = aCellsArray[spannedRowIdx][colIdx];
            if (!isRowSpannedCell(spannedCellState)) {
              aCellsArray[rowIdx][colIdx] = false;
              break;
            }
          }
        }
      }
    }
  }

  acc.selectRow(aRowIdx);
  testTableSelection(aIdentifier, aCellsArray,
                     "Select " + aRowIdx + " row: ");
}

////////////////////////////////////////////////////////////////////////////////
// private implementation

/**
 * Return row and column of orig cell for the given spanned cell.
 */
function getOrigRowAndColumn(aCellsArray, aRowIdx, aColIdx)
{
  var cellState = aCellsArray[aRowIdx][aColIdx];

  var origRowIdx = aRowIdx, origColIdx = aColIdx;
  if (isRowSpannedCell(cellState)) {
    for (var prevRowIdx = aRowIdx - 1; prevRowIdx >= 0; prevRowIdx--) {
      var prevCellState = aCellsArray[prevRowIdx][aColIdx];
      if (!isRowSpannedCell(prevCellState)) {
        origRowIdx = prevRowIdx;
        break;
      }
    }
  }

  if (isColSpannedCell(cellState)) {
    for (var prevColIdx = aColIdx - 1; prevColIdx >= 0; prevColIdx--) {
      var prevCellState = aCellsArray[aRowIdx][prevColIdx];
      if (!isColSpannedCell(prevCellState)) {
        origColIdx = prevColIdx;
        break;
      }
    }
  }

  return [origRowIdx, origColIdx];
}

function isOrigCell(aCellState)
{
  return !(aCellState & (kRowSpanned | kColSpanned));
}

function isRowSpannedCell(aCellState)
{
  return aCellState & kRowSpanned;
}

function isColSpannedCell(aCellState)
{
  return aCellState & kColSpanned;
}
