/**
 * This file provides set of helper functions to test nsIAccessibleTable
 * interface.
 *
 * Required:
 *   common.js
 *   role.js
 *   states.js
 */

/**
 * Constants used to describe cells array.
 */
const kDataCell = 1; // Indicates the cell is origin data cell
const kRowHeaderCell = 2; // Indicates the cell is row header cell
const kColHeaderCell = 4; // Indicated the cell is column header cell
const kOrigin = kDataCell | kRowHeaderCell | kColHeaderCell;

const kRowSpanned = 8; // Indicates the cell is not origin and row spanned 
const kColSpanned = 16; // Indicates the cell is not origin and column spanned
const kSpanned = kRowSpanned | kColSpanned;

/**
 * Constants to define column header type.
 */
const kNoColumnHeader = 0;
const kListboxColumnHeader = 1;
const kTreeColumnHeader = 2;

/**
 * Constants to define table type.
 */
const kTable = 0;
const kTreeTable = 1;
const kMathTable = 2;

/**
 * Test table structure and related methods.
 *
 * @param  aIdentifier     [in] table accessible identifier
 * @param  aCellsArray     [in] two dimensional array (row X columns) of
 *                          cell types (see constants defined above).
 * @param  aColHeaderType  [in] specifies wether column header cells are
 *                          arranged into the list.
 * @param  aCaption        [in] caption text if any
 * @param  aSummary        [in] summary text if any
 * @param  aTableType      [in] specifies the table type.
 * @param  aRowRoles       [in] array of row roles.
 */
function testTableStruct(aIdentifier, aCellsArray, aColHeaderType,
                         aCaption, aSummary, aTableType, aRowRoles)
{
  var tableNode = getNode(aIdentifier);
  var isGrid = tableNode.getAttribute("role") == "grid" ||
    tableNode.getAttribute("role") == "treegrid" ||
    tableNode.localName == "tree";

  var rowCount = aCellsArray.length;
  var colsCount = aCellsArray[0] ? aCellsArray[0].length : 0;

  // Test table accessible tree.
  var tableObj = {
    children: []
  };
  switch (aTableType) {
    case kTable:
      tableObj.role = ROLE_TABLE;
      break;
    case kTreeTable:
      tableObj.role = ROLE_TREE_TABLE;
      break;
    case kMathTable:
      tableObj.role = ROLE_MATHML_TABLE;
      break;
  }

  // caption accessible handling
  if (aCaption) {
    var captionObj = {
      role: ROLE_CAPTION,
      children: [
        {
          role: ROLE_TEXT_LEAF,
          name: aCaption
        }
      ]
    };

    tableObj.children.push(captionObj);
  }

  // special types of column headers handling
  if (aColHeaderType) {
    var headersObj = {
      role: ROLE_LIST,
      children: []
    };

    for (var idx = 0; idx < colsCount; idx++) {
      var headerCellObj = {
        role: ROLE_COLUMNHEADER
      };
      headersObj.children.push(headerCellObj);
    }

    if (aColHeaderType == kTreeColumnHeader) {
      var columnPickerObj = {
        role: ROLE_PUSHBUTTON
      };

      headersObj.children.push(columnPickerObj);
    }

    tableObj.children.push(headersObj);
  }

  // rows and cells accessibles
  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    var rowObj = {
      role: aRowRoles ? aRowRoles[rowIdx] : ROLE_ROW,
      children: []
    };

    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      var celltype = aCellsArray[rowIdx][colIdx];

      var role = ROLE_NOTHING;
      switch (celltype) {
        case kDataCell:
          role = (aTableType == kMathTable ? ROLE_MATHML_CELL :
                  (isGrid ? ROLE_GRID_CELL : ROLE_CELL));
          break;
        case kRowHeaderCell:
          role = ROLE_ROWHEADER;
          break;
        case kColHeaderCell:
          role = ROLE_COLUMNHEADER;
          break;
      }

      if (role != ROLE_NOTHING) {
        var cellObj = {
          role: role
        };
        rowObj.children.push(cellObj);
      }
    }

    tableObj.children.push(rowObj);
  }

  testAccessibleTree(aIdentifier, tableObj);

  // Test table table interface.
  var table = getAccessible(aIdentifier, [nsIAccessibleTable]);

  // summary
  if (aSummary)
    is(table.summary, aSummary,
       "Wrong summary of the table " + prettyName(aIdentifier));

  // rowCount and columnCount
  is(table.rowCount, rowCount,
     "Wrong rows count of " + prettyName(aIdentifier));
  is(table.columnCount, colsCount,
     "Wrong columns count of " + prettyName(aIdentifier));

  // rows and columns extents
  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      var celltype = aCellsArray[rowIdx][colIdx];
      if (celltype & kOrigin) {

        // table getRowExtentAt
        var rowExtent = table.getRowExtentAt(rowIdx, colIdx);
        for (var idx = rowIdx + 1;
             idx < rowCount && (aCellsArray[idx][colIdx] & kRowSpanned);
             idx++);

        var expectedRowExtent = idx - rowIdx;
        is(rowExtent, expectedRowExtent,
           "getRowExtentAt: Wrong number of spanned rows at (" + rowIdx + ", " +
           colIdx + ") for " + prettyName(aIdentifier));

        // table getColumnExtentAt
        var colExtent = table.getColumnExtentAt(rowIdx, colIdx);
        for (var idx = colIdx + 1;
             idx < colsCount && (aCellsArray[rowIdx][idx] & kColSpanned);
             idx++);

        var expectedColExtent = idx - colIdx;
        is(colExtent, expectedColExtent,
           "getColumnExtentAt: Wrong number of spanned columns at (" + rowIdx +
           ", " + colIdx + ") for " + prettyName(aIdentifier));

        // cell rowExtent and columnExtent
        var cell = getAccessible(table.getCellAt(rowIdx, colIdx),
                                 [nsIAccessibleTableCell]);

        is(cell.rowExtent, expectedRowExtent,
           "rowExtent: Wrong number of spanned rows at (" + rowIdx + ", " +
           colIdx + ") for " + prettyName(aIdentifier));

        is(cell.columnExtent, expectedColExtent,
           "columnExtent: Wrong number of spanned column at (" + rowIdx + ", " +
           colIdx + ") for " + prettyName(aIdentifier));
      }
    }
  }
}

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

      // getCellAt
      try {
        cellAcc = null;
        cellAcc = tableAcc.getCellAt(rowIdx, colIdx);
      } catch (e) { }

      ok(idx != -1 && cellAcc || idx == -1 && !cellAcc,
         id + ": Can't get cell accessible at row = " + rowIdx + ", column = " + colIdx);

      if (idx != - 1) {

        // getRowIndexAt
        var origRowIdx = rowIdx;
        while (origRowIdx > 0 &&
               aIdxes[rowIdx][colIdx] == aIdxes[origRowIdx - 1][colIdx])
          origRowIdx--;

        try {
          obtainedRowIdx = tableAcc.getRowIndexAt(idx);
        } catch (e) {
          ok(false, id + ": can't get row index for cell index " + idx + "," + e);
        }

        is(obtainedRowIdx, origRowIdx,
           id + ": row for index " + idx + " is not correct (getRowIndexAt)");

        // getColumnIndexAt
        var origColIdx = colIdx;
        while (origColIdx > 0 &&
               aIdxes[rowIdx][colIdx] == aIdxes[rowIdx][origColIdx - 1])
          origColIdx--;

        try {
          obtainedColIdx = tableAcc.getColumnIndexAt(idx);
        } catch (e) {
          ok(false, id + ": can't get column index for cell index " + idx + "," + e);
        }

        is(obtainedColIdx, origColIdx,
           id + ": column  for index " + idx + " is not correct (getColumnIndexAt)");

        // getRowAndColumnIndicesAt
        var obtainedRowIdxObj = { }, obtainedColIdxObj = { };
        try {
          tableAcc.getRowAndColumnIndicesAt(idx, obtainedRowIdxObj,
                                            obtainedColIdxObj);
        } catch (e) {
          ok(false, id + ": can't get row and column indices for cell index " + idx + "," + e);
        }

        is(obtainedRowIdxObj.value, origRowIdx,
           id + ": row for index " + idx + " is not correct (getRowAndColumnIndicesAt)");
        is(obtainedColIdxObj.value, origColIdx,
           id + ": column  for index " + idx + " is not correct (getRowAndColumnIndicesAt)");

        if (cellAcc) {

          var cellId = prettyName(cellAcc);
          cellAcc = getAccessible(cellAcc, [nsIAccessibleTableCell]);

          // cell: 'table-cell-index' attribute
          var attrs = cellAcc.attributes;
          var strIdx = "";
          try {
            strIdx = attrs.getStringProperty("table-cell-index");
          } catch (e) {
            ok(false,
               cellId + ": no cell index from object attributes on the cell accessible at index " + idx + ".");
          }

          if (strIdx) {
            is (parseInt(strIdx), idx,
                cellId + ": cell index from object attributes of cell accessible isn't corrent.");
          }

          // cell: table
          try {
            is(cellAcc.table, tableAcc,
               cellId + ": wrong table accessible for the cell.");

          } catch (e) {
            ok(false,
               cellId + ": can't get table accessible from the cell.");
          }

          // cell: getRowIndex
          try {
            obtainedRowIdx = cellAcc.rowIndex;
          } catch (e) {
            ok(false,
               cellId + ": can't get row index of the cell at index " + idx + "," + e);
          }

          is(obtainedRowIdx, origRowIdx,
             cellId + ": row for the cell at index " + idx +" is not correct");

          // cell: getColumnIndex
          try {
            obtainedColIdx = cellAcc.columnIndex;
          } catch (e) {
            ok(false,
               cellId + ": can't get column index of the cell at index " + idx + "," + e);
          }

          is(obtainedColIdx, origColIdx,
             id + ": column for the cell at index " + idx +" is not correct");
        }
      }

      // getCellIndexAt
      try {
        obtainedIdx = tableAcc.getCellIndexAt(rowIdx, colIdx);
      } catch (e) {
        obtainedIdx = -1;
      }

      is(obtainedIdx, idx,
         id + ": row " + rowIdx + " /column " + colIdx + " and index " + obtainedIdx + " aren't inconsistent.");
    }
  }
}

/**
 * Test table getters selection methods.
 *
 * @param  aIdentifier  [in] table accessible identifier
 * @param  aCellsArray  [in] two dimensional array (row X columns) of cells
 *                       states (either boolean (selected/unselected) if cell is
 *                       origin, otherwise kRowSpanned or kColSpanned constant).
 * @param  aMsg         [in] text appended before every message
 */
function testTableSelection(aIdentifier, aCellsArray, aMsg)
{
  var msg = aMsg ? aMsg : "";
  var acc = getAccessible(aIdentifier, [nsIAccessibleTable]);
  if (!acc)
    return;

  var rowCount = aCellsArray.length;
  var colsCount = aCellsArray[0].length;

  // Columns selection tests.
  var selCols = new Array();

  // isColumnSelected test
  for (var colIdx = 0; colIdx < colsCount; colIdx++) {
    var isColSelected = true;
    for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
      if (aCellsArray[rowIdx][colIdx] == false ||
          aCellsArray[rowIdx][colIdx] == undefined) {
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
  is(acc.selectedColumnCount, selCols.length,
     msg + "Wrong count of selected columns for " + prettyName(aIdentifier));

  // getSelectedColumns test
  var actualSelColsCountObj = { value: null };
  var actualSelCols = acc.getSelectedColumnIndices(actualSelColsCountObj);

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
  var selrowCount = 0;
  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    var isRowSelected = true;
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      if (aCellsArray[rowIdx][colIdx] == false ||
          aCellsArray[rowIdx][colIdx] == undefined) {
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

  // selectedRowCount test
  is(acc.selectedRowCount, selRows.length,
     msg + "Wrong count of selected rows for " + prettyName(aIdentifier));

  // getSelectedRows test
  var actualSelrowCountObj = { value: null };
  var actualSelRows = acc.getSelectedRowIndices(actualSelrowCountObj);

  var actualSelrowCount = actualSelrowCountObj.value;
  is (actualSelrowCount, selRows.length,
      msg + "Wrong count of selected rows for " + prettyName(aIdentifier) +
      "from getSelectedRows.");

  for (var i = 0; i < actualSelrowCount; i++) {
    is (actualSelRows[i], selRows[i],
        msg + "Row at index " + selRows[i] + " should be selected.");
  }

  // Cells selection tests.
  var selCells = new Array();

  // isCellSelected test
  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      if (aCellsArray[rowIdx][colIdx] & kSpanned)
        continue;

      var isSelected = aCellsArray[rowIdx][colIdx] == true;
      is(acc.isCellSelected(rowIdx, colIdx), isSelected,
         msg + "Wrong selection state of cell at " + rowIdx + " row and " +
         colIdx + " column for " + prettyName(aIdentifier));

      if (aCellsArray[rowIdx][colIdx])
        selCells.push(acc.getCellIndexAt(rowIdx, colIdx));
    }
  }

  // selectedCellCount tests
  is(acc.selectedCellCount, selCells.length,
     msg + "Wrong count of selected cells for " + prettyName(aIdentifier));

  // getSelectedCellIndices test
  var actualSelCellsCountObj = { value: null };
  var actualSelCells = acc.getSelectedCellIndices(actualSelCellsCountObj);

  var actualSelCellsCount = actualSelCellsCountObj.value;
  is(actualSelCellsCount, selCells.length,
     msg + "Wrong count of selected cells for " + prettyName(aIdentifier) +
     "from getSelectedCells.");

  for (var i = 0; i < actualSelCellsCount; i++) {
    is(actualSelCells[i], selCells[i],
       msg + "getSelectedCellIndices: Cell at index " + selCells[i] +
       " should be selected.");
  }

  // selectedCells and isSelected tests
  var actualSelCellsArray = acc.selectedCells;
  for (var i = 0; i < actualSelCellsCount; i++) {
    var actualSelCellAccessible =
      actualSelCellsArray.queryElementAt(i, nsIAccessibleTableCell);

    var colIdx = acc.getColumnIndexAt(selCells[i]);
    var rowIdx = acc.getRowIndexAt(selCells[i]);
    var expectedSelCellAccessible = acc.getCellAt(rowIdx, colIdx);

    ok(actualSelCellAccessible, expectedSelCellAccessible,
       msg + "getSelectedCells: Cell at index " + selCells[i] +
       " should be selected.");

    ok(actualSelCellAccessible.isSelected(),
       "isSelected: Cell at index " + selCells[i] + " should be selected.");
  }

  // selected states tests
  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      if (aCellsArray[rowIdx][colIdx] & kSpanned)
        continue;

      var cell = acc.getCellAt(rowIdx, colIdx);
      var isSel = aCellsArray[rowIdx][colIdx];
      if (isSel == undefined)
        testStates(cell, 0, 0, STATE_SELECTABLE | STATE_SELECTED);
      else if (isSel == true)
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

  var rowCount = aCellsArray.length;
  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
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

  var rowCount = aCellsArray.length;
  var colsCount = aCellsArray[0].length;

  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      var cellState = aCellsArray[rowIdx][colIdx];

      if (colIdx == aColIdx) { // select target column
        if (!(cellState & kSpanned)) {
          // Select the cell if it is origin.
          aCellsArray[rowIdx][colIdx] = true;

        } else {
          // If the cell is spanned then search origin cell and select it.
          var [origRowIdx, origColIdx] = getOrigRowAndColumn(aCellsArray,
                                                             rowIdx, colIdx);
          aCellsArray[origRowIdx][origColIdx] = true;
        }

      } else if (!(cellState & kSpanned)) { // unselect other columns
        if (colIdx > aColIdx) {
          // Unselect the cell if traversed column index is greater than column
          // index of target cell.
          aCellsArray[rowIdx][colIdx] = false;

        } else if (!(aCellsArray[rowIdx][aColIdx] & kColSpanned)) {
          // Unselect the cell if the target cell is not row spanned.
          aCellsArray[rowIdx][colIdx] = false;

        } else {
          // Unselect the cell if it is not spanned to the target cell.
          for (var spannedColIdx = colIdx + 1; spannedColIdx < aColIdx;
               spannedColIdx++) {
            var spannedCellState = aCellsArray[rowIdx][spannedColIdx];
            if (!(spannedCellState & kRowSpanned)) {
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

  var rowCount = aCellsArray.length;
  var colsCount = aCellsArray[0].length;

  for (var rowIdx = 0; rowIdx < rowCount; rowIdx++) {
    for (var colIdx = 0; colIdx < colsCount; colIdx++) {
      var cellState = aCellsArray[rowIdx][colIdx];

      if (rowIdx == aRowIdx) { // select the given row
        if (!(cellState & kSpanned)) {
          // Select the cell if it is origin.
          aCellsArray[rowIdx][colIdx] = true;

        } else {
          // If the cell is spanned then search origin cell and select it.
          var [origRowIdx, origColIdx] = getOrigRowAndColumn(aCellsArray,
                                                             rowIdx, colIdx);

          aCellsArray[origRowIdx][origColIdx] = true;
        }

      } else if (!(cellState & kSpanned)) { // unselect other rows
        if (rowIdx > aRowIdx) {
          // Unselect the cell if traversed row index is greater than row
          // index of target cell.
          aCellsArray[rowIdx][colIdx] = false;

        } else if (!(aCellsArray[aRowIdx][colIdx] & kRowSpanned)) {
          // Unselect the cell if the target cell is not row spanned.
          aCellsArray[rowIdx][colIdx] = false;

        } else {
          // Unselect the cell if it is not spanned to the target cell.
          for (var spannedRowIdx = rowIdx + 1; spannedRowIdx < aRowIdx;
               spannedRowIdx++) {
            var spannedCellState = aCellsArray[spannedRowIdx][colIdx];
            if (!(spannedCellState & kRowSpanned)) {
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

/**
 * Test columnHeaderCells and rowHeaderCells of accessible table.
 */
function testHeaderCells(aHeaderInfoMap)
{
  for (var testIdx = 0; testIdx < aHeaderInfoMap.length; testIdx++) {
    var dataCellIdentifier = aHeaderInfoMap[testIdx].cell;
    var dataCell = getAccessible(dataCellIdentifier, [nsIAccessibleTableCell]);

    // row header cells
    var rowHeaderCells = aHeaderInfoMap[testIdx].rowHeaderCells;
    var rowHeaderCellsCount = rowHeaderCells.length;
    var actualRowHeaderCells = dataCell.rowHeaderCells;
    var actualRowHeaderCellsCount = actualRowHeaderCells.length;

    is(actualRowHeaderCellsCount, rowHeaderCellsCount,
       "Wrong number of row header cells for the cell " +
       prettyName(dataCellIdentifier));

    if (actualRowHeaderCellsCount == rowHeaderCellsCount) {
      for (var idx = 0; idx < rowHeaderCellsCount; idx++) {
        var rowHeaderCell = getAccessible(rowHeaderCells[idx]);
        var actualRowHeaderCell =
          actualRowHeaderCells.queryElementAt(idx, nsIAccessible);
        isObject(actualRowHeaderCell, rowHeaderCell,
                 "Wrong row header cell at index " + idx + " for the cell " +
                 dataCellIdentifier);
      }
    }

    // column header cells
    var colHeaderCells = aHeaderInfoMap[testIdx].columnHeaderCells;
    var colHeaderCellsCount = colHeaderCells.length;
    var actualColHeaderCells = dataCell.columnHeaderCells;
    var actualColHeaderCellsCount = actualColHeaderCells.length;

    is(actualColHeaderCellsCount, colHeaderCellsCount,
       "Wrong number of column header cells for the cell " +
       prettyName(dataCellIdentifier));

    if (actualColHeaderCellsCount == colHeaderCellsCount) {
      for (var idx = 0; idx < colHeaderCellsCount; idx++) {
        var colHeaderCell = getAccessible(colHeaderCells[idx]);
        var actualColHeaderCell =
          actualColHeaderCells.queryElementAt(idx, nsIAccessible);
        isObject(actualColHeaderCell, colHeaderCell,
                 "Wrong column header cell at index " + idx + " for the cell " +
                 dataCellIdentifier);
      }
    }
  }
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
  if (cellState & kRowSpanned) {
    for (var prevRowIdx = aRowIdx - 1; prevRowIdx >= 0; prevRowIdx--) {
      var prevCellState = aCellsArray[prevRowIdx][aColIdx];
      if (!(prevCellState & kRowSpanned)) {
        origRowIdx = prevRowIdx;
        break;
      }
    }
  }

  if (cellState & kColSpanned) {
    for (var prevColIdx = aColIdx - 1; prevColIdx >= 0; prevColIdx--) {
      var prevCellState = aCellsArray[aRowIdx][prevColIdx];
      if (!(prevCellState & kColSpanned)) {
        origColIdx = prevColIdx;
        break;
      }
    }
  }

  return [origRowIdx, origColIdx];
}
