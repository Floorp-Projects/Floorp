/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Ben Goodger
 */

//Cancel() is in EdDialogCommon.js
var tagname = "table"
var TableElement;
var CellElement;
var TableCaptionElement;
var TabPanels;
var dialog;
var globalCellElement;
var globalTableElement
var TablePanel = 0;
var CellPanel = 1;
var currentPanel = TablePanel;
var validatePanel;
var defHAlign =   "left";
var centerStr =   "center";  //Index=1
var rightStr =    "right";   // 2
var justifyStr =  "justify"; // 3
var charStr =     "char";    // 4
var defVAlign =   "middle";
var topStr =      "top";
var bottomStr =   "bottom";
var bgcolor = "bgcolor";
var TableColor;
var CellColor;

var rowCount = 1;
var colCount = 1;
var lastRowIndex;
var lastColIndex;
var newRowCount;
var newColCount;
var curRowIndex;
var curColIndex;
var curColSpan;
var SelectedCellsType = 1;
var SELECT_CELL = 1;
var SELECT_ROW = 2;
var SELECT_COLUMN = 3;
var RESET_SELECTION = 0;
var cellData = new Object;
var AdvancedEditUsed;
var alignWasChar = false;

/*
From C++:
 0 TABLESELECTION_TABLE
 1 TABLESELECTION_CELL   There are 1 or more cells selected
                          but complete rows or columns are not selected
 2 TABLESELECTION_ROW    All cells are in 1 or more rows
                          and in each row, all cells selected
                          Note: This is the value if all rows (thus all cells) are selected
 3 TABLESELECTION_COLUMN All cells are in 1 or more columns
*/

var gSelectedCellCount = 0;
var ApplyUsed = false;
// What should these be?
var maxRows    = 1000; // This is the value gecko code uses for maximum rowspan, colspan
var maxColumns = 1000;
var selection;
var CellDataChanged = false;
var canDelete = false;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell()) return;

  selection = editorShell.editorSelection;
  if (!selection) return;

  dialog = new Object;
  if (!dialog)
  {
    window.close();
    return;
  }
  // Get dialog widgets - Table Panel
  dialog.TableRowsInput = document.getElementById("TableRowsInput");
  dialog.TableColumnsInput = document.getElementById("TableColumnsInput");
  dialog.TableWidthInput = document.getElementById("TableWidthInput");
  dialog.TableWidthUnits = document.getElementById("TableWidthUnits");
  dialog.BorderWidthInput = document.getElementById("BorderWidthInput");
  dialog.SpacingInput = document.getElementById("SpacingInput");
  dialog.PaddingInput = document.getElementById("PaddingInput");
  dialog.TableAlignList = document.getElementById("TableAlignList");
  dialog.TableCaptionList = document.getElementById("TableCaptionList");
  dialog.TableInheritColor = document.getElementById("TableInheritColor");

  // Cell Panel
  dialog.SelectionList = document.getElementById("SelectionList");
  dialog.PreviousButton = document.getElementById("PreviousButton");
  dialog.NextButton = document.getElementById("NextButton");
  // Currently, we always apply changes and load new attributes when changing selection
  // (Let's keep this for possible future use)
  //dialog.ApplyBeforeMove =  document.getElementById("ApplyBeforeMove");
  //dialog.KeepCurrentData = document.getElementById("KeepCurrentData");

  dialog.CellHeightInput = document.getElementById("CellHeightInput");
  dialog.CellHeightUnits = document.getElementById("CellHeightUnits");
  dialog.CellWidthInput = document.getElementById("CellWidthInput");
  dialog.CellWidthUnits = document.getElementById("CellWidthUnits");
  dialog.CellHAlignList = document.getElementById("CellHAlignList");
  dialog.CellVAlignList = document.getElementById("CellVAlignList");
  dialog.CellInheritColor = document.getElementById("CellInheritColor");
  dialog.CellStyleList = document.getElementById("CellStyleList");
  dialog.TextWrapList = document.getElementById("TextWrapList");

  // In cell panel, user must tell us which attributes to apply via checkboxes,
  //  else we would apply values from one cell to ALL in selection
  //  and that's probably not what they expect!
  dialog.CellHeightCheckbox = document.getElementById("CellHeightCheckbox");
  dialog.CellWidthCheckbox = document.getElementById("CellWidthCheckbox");
  dialog.CellHAlignCheckbox = document.getElementById("CellHAlignCheckbox");
  dialog.CellVAlignCheckbox = document.getElementById("CellVAlignCheckbox");
  dialog.CellStyleCheckbox = document.getElementById("CellStyleCheckbox");
  dialog.TextWrapCheckbox = document.getElementById("TextWrapCheckbox");
  dialog.CellColorCheckbox = document.getElementById("CellColorCheckbox");

  TabPanels = document.getElementById("TabPanels");
  var TableTab = document.getElementById("TableTab");
  var CellTab = document.getElementById("CellTab");

  TableElement = editorShell.GetElementOrParentByTagName("table", null);
  if(!TableElement)
  {
    dump("Failed to get table element!\n");
    window.close();
    return;
  }
  globalTableElement = TableElement.cloneNode(false);

  var tagNameObj = new Object;
  var countObj = new Object;
  var tableOrCellElement = editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);
  gSelectedCellCount = countObj.value;

  if (tagNameObj.value == "td")
  {
    // We are in a cell
    CellElement = tableOrCellElement;
    globalCellElement = CellElement.cloneNode(false);

    // Tells us whether cell, row, or column is selected
    SelectedCellsType = editorShell.GetSelectedCellsType(TableElement);

    // Ignore types except Cell, Row, and Column
    if (SelectedCellsType < SELECT_CELL || SelectedCellsType > SELECT_COLUMN)
      SelectedCellsType = SELECT_CELL;

    // Be sure at least 1 cell is selected.
    // (If the count is 0, then we were inside the cell.)
    if (gSelectedCellCount == 0)
      DoCellSelection();

    // Get location in the cell map
    curRowIndex = editorShell.GetRowIndex(CellElement);
    curColIndex = editorShell.GetColumnIndex(CellElement);

    // We save the current colspan to quickly
    //  move selection from from cell to cell
    if (GetCellData(curRowIndex, curColIndex))
      curColSpan = cellData.colSpan;

    // Starting TabPanel name is passed in
    if (window.arguments[1] == "CellPanel")
    {
      currentPanel = CellPanel;

      //Set index for starting panel on the <tabpanels> element
      TabPanels.setAttribute("index", CellPanel);

      // Trigger setting of style for the tab widgets
      CellTab.setAttribute("selected", "true");
      TableTab.removeAttribute("selected");

      // Use cell element for Advanced Edit dialog
      globalElement = globalCellElement;
    }
  }

  if (currentPanel == TablePanel)
  {
    // Use table element for Advanced Edit dialog
    globalElement = globalTableElement;

    // We may call this with table selected, but no cell,
    //  so disable the Cell Properties tab
    if(!CellElement)
    {
      // XXX: Disabling of tabs is currently broken, so for
      //      now we'll just remove the tab completely.
      //CellTab.setAttribute("disabled", "true");
      CellTab.parentNode.removeChild(CellTab);
    }
  }

  doSetOKCancel(onOK, onCancel, 0, onApply);

  // Note: we must use TableElement, not globalTableElement for these,
  //  thus we should not put this in InitDialog.
  // Instead, monitor desired counts with separate globals
  rowCount = editorShell.GetTableRowCount(TableElement);
  lastRowIndex = rowCount-1;
  colCount = editorShell.GetTableColumnCount(TableElement);
  lastColIndex = colCount-1;


  // Set appropriate icons and enable state for the Previous/Next buttons
  SetSelectionButtons();

  // If only one cell in table, disable change-selection widgets
  if (rowCount == 1 && colCount == 1)
    dialog.SelectionList.setAttribute("disabled", "true");

  // User can change these via textboxes
  newRowCount = rowCount;
  newColCount = colCount;

  // This flag is used to control whether set check state
  //  on "set attribute" checkboxes
  // (Advanced Edit dialog use calls  InitDialog when done)
  AdvancedEditUsed = false;
  InitDialog();
  AdvancedEditUsed = true;

  // If first initializing, we really aren't changing anything
  CellDataChanged = false;

  if (currentPanel == CellPanel)
    dialog.SelectionList.focus();
  else
    SetTextboxFocus(dialog.TableRowsInput);

  SetWindowLocation();
}


function InitDialog()
{
// turn on Button3 to be "apply"
  var applyButton = document.getElementById("Button3");
  if (applyButton)
  {
    applyButton.label = GetString("Apply");
    applyButton.removeAttribute("collapsed");
  }
  
  // Get Table attributes
  dialog.TableRowsInput.value = rowCount;
  dialog.TableColumnsInput.value = colCount;
  dialog.TableWidthInput.value = InitPixelOrPercentMenulist(globalTableElement, TableElement, "width", "TableWidthUnits", gPercent);
  dialog.BorderWidthInput.value = globalTableElement.border;
  dialog.SpacingInput.value = globalTableElement.cellSpacing;
  dialog.PaddingInput.value = globalTableElement.cellPadding;

  //BUG: The align strings are converted: e.g., "center" becomes "Center";
  var halign = globalTableElement.align.toLowerCase();
  if (halign == centerStr)
    dialog.TableAlignList.selectedIndex = 1;
  else if (halign == rightStr)
    dialog.TableAlignList.selectedIndex = 2;
  else // Default = left
    dialog.TableAlignList.selectedIndex = 0;

  // Be sure to get caption from table in doc, not the copied "globalTableElement"
  TableCaptionElement = TableElement.caption;
  var index = 0;
  if (TableCaptionElement)
  {
    // Note: Other possible values are "left" and "right",
    //  but "align" is deprecated, so should we even support "botton"?
    if (TableCaptionElement.vAlign == "bottom")
      index = 2;
    else
      index = 1;
  }
  dialog.TableCaptionList.selectedIndex = index;

  TableColor = globalTableElement.bgColor;
  SetColor("tableBackgroundCW", TableColor);

  InitCellPanel();
}

function InitCellPanel()
{
  // Get cell attributes
  if (globalCellElement)
  {
    // This assumes order of items is Cell, Row, Column
    dialog.SelectionList.selectedIndex = SelectedCellsType-1;

    var previousValue = dialog.CellHeightInput.value;
    dialog.CellHeightInput.value = InitPixelOrPercentMenulist(globalCellElement, CellElement, "height", "CellHeightUnits", gPixel);
    dialog.CellHeightCheckbox.checked = AdvancedEditUsed && previousValue != dialog.CellHeightInput.value;

    previousValue= dialog.CellWidthInput.value;
    dialog.CellWidthInput.value = InitPixelOrPercentMenulist(globalCellElement, CellElement, "width", "CellWidthUnits", gPixel);
    dialog.CellWidthCheckbox.checked = AdvancedEditUsed && previousValue != dialog.CellWidthInput.value;

    var previousIndex = dialog.CellVAlignList.selectedIndex;
    var valign = globalCellElement.vAlign.toLowerCase();
    if (valign == topStr)
      dialog.CellVAlignList.selectedIndex = 0;
    else if (valign == bottomStr)
      dialog.CellVAlignList.selectedIndex = 2;
    else // Default = middle
      dialog.CellVAlignList.selectedIndex = 1;

    dialog.CellVAlignCheckbox.checked = AdvancedEditUsed && previousValue != dialog.CellVAlignList.selectedIndex;


    previousIndex = dialog.CellHAlignList.selectedIndex;

    alignWasChar = false;

    var halign = globalCellElement.align.toLowerCase();
    switch (halign)
    {
      case centerStr:
        dialog.CellHAlignList.selectedIndex = 1;
        break;
      case rightStr:
        dialog.CellHAlignList.selectedIndex = 2;
        break;
      case justifyStr:
        dialog.CellHAlignList.selectedIndex = 3;
        break;
      case charStr:
        // We don't support UI for this because layout doesn't work: bug 2212.
        // Remember that's what they had so we don't change it
        //  unless they change the alignment by using the menulist
        alignWasChar = true;
        // Fall through to use show default alignment in menu
      default:
        // Default depends on cell type (TH is "center", TD is "left")
        dialog.CellHAlignList.selectedIndex =
          (globalCellElement.nodeName.toLowerCase() == "th") ? 1 : 0;
        break;
    }

    dialog.CellHAlignCheckbox.checked = AdvancedEditUsed &&
      previousIndex != dialog.CellHAlignList.selectedIndex;

    previousIndex = dialog.CellStyleList.selectedIndex;
    dialog.CellStyleList.selectedIndex = (globalCellElement.nodeName.toLowerCase() == "th") ? 1 : 0;
    dialog.CellStyleCheckbox.checked = AdvancedEditUsed && previousIndex != dialog.CellStyleList.selectedIndex;

    previousIndex = dialog.TextWrapList.selectedIndex;
    dialog.TextWrapList.selectedIndex = globalCellElement.noWrap ? 1 : 0;
    dialog.TextWrapCheckbox.checked = AdvancedEditUsed && previousIndex != dialog.TextWrapList.selectedIndex;

    previousValue = CellColor;
    SetColor("cellBackgroundCW", globalCellElement.bgColor);
    dialog.CellColorCheckbox.checked = AdvancedEditUsed && CellColor != globalCellElement.bgColor;
    CellColor = globalCellElement.bgColor;

    // We want to set this true in case changes came
    //   from Advanced Edit dialog session (must assume something changed)
    CellDataChanged = true;
  }
}

function GetCellData(rowIndex, colIndex)
{
  // Get actual rowspan and colspan
  var startRowIndexObj = new Object;
  var startColIndexObj = new Object;
  var rowSpanObj = new Object;
  var colSpanObj = new Object;
  var actualRowSpanObj = new Object;
  var actualColSpanObj = new Object;
  var isSelectedObj = new Object;
  if (!cellData)
    cellData = new Object;

  try {
    cellData.cell =
      editorShell.GetCellDataAt(TableElement, rowIndex, colIndex,
                                startRowIndexObj, startColIndexObj,
                                rowSpanObj, colSpanObj,
                                actualRowSpanObj, actualColSpanObj, isSelectedObj);
    // We didn't find a cell
    if (!cellData.cell) return false;
  }
  catch(ex) {
    return false;
  }

  cellData.startRowIndex = startRowIndexObj.value;
  cellData.startColIndex = startColIndexObj.value;
  cellData.rowSpan = rowSpanObj.value;
  cellData.colSpan = colSpanObj.value;
  cellData.actualRowSpan = actualRowSpanObj.value;
  cellData.actualColSpan = actualColSpanObj.value;
  cellData.isSelected = isSelectedObj.value;
  return true;
}

function SelectTableTab()
{
  globalElement = globalTableElement;
  currentPanel = TablePanel;
}

function SelectCellTab()
{
  globalElement = globalCellElement;
  currentPanel = CellPanel;
}

function SelectCellHAlign()
{
  SetCheckbox("CellHAlignCheckbox");
  // Once user changes the alignment,
  //  we loose their original "CharAt" alignment"
  alignWasChar = false;
}

function GetColorAndUpdate(ColorWellID)
{
  var colorWell = document.getElementById(ColorWellID);
  if (!colorWell) return;

  var colorObj = new Object;

  switch( ColorWellID )
  {
    case "tableBackgroundCW":
      colorObj.Type = "Table";
      colorObj.TableColor = TableColor;
      break;
    case "cellBackgroundCW":
      colorObj.Type = "Cell";
      colorObj.CellColor = CellColor;
      break;
  }
  // Avoid the JS warning
  colorObj.NoDefault = false;
  window.openDialog("chrome://editor/content/EdColorPicker.xul", "_blank", "chrome,close,titlebar,modal", "", colorObj);

  // User canceled the dialog
  if (colorObj.Cancel)
    return;

  switch( ColorWellID )
  {
    case "tableBackgroundCW":
      TableColor = colorObj.BackgroundColor;
      SetColor(ColorWellID, TableColor);
      break;
    case "cellBackgroundCW":
      CellColor = colorObj.BackgroundColor;
      SetColor(ColorWellID, CellColor);
      SetCheckbox('CellColorCheckbox');
      break;
  }
}

function SetColor(ColorWellID, color)
{
  // Save the color
  if (ColorWellID == "cellBackgroundCW")
  {
    if (color)
    {
      globalCellElement.setAttribute(bgcolor, color);
      dialog.CellInheritColor.setAttribute("collapsed","true");
    }
    else
    {
      globalCellElement.removeAttribute(bgcolor);
      // Reveal addition message explaining "default" color
      dialog.CellInheritColor.removeAttribute("collapsed");
    }
  }
  else
  {
    if (color)
    {
      globalTableElement.setAttribute(bgcolor, color);
      dialog.TableInheritColor.setAttribute("collapsed","true");
    }
    else
    {
      globalTableElement.removeAttribute(bgcolor);
      dialog.TableInheritColor.removeAttribute("collapsed");
    }
    SetCheckbox('CellColorCheckbox');
  }

  setColorWell(ColorWellID, color);
}

function ChangeSelectionToFirstCell()
{
  if (!GetCellData(0,0))
  {
    dump("Can't find first cell in table!\n");
    return;
  }
  CellElement = cellData.cell;
  globalCellElement = CellElement;
  globalElement = CellElement;

  curRowIndex = 0;
  curColIndex = 0;
  ChangeSelection(RESET_SELECTION);
}

function ChangeSelection(newType)
{
  newType = Number(newType);

  if (SelectedCellsType == newType)
    return;

  if (newType == RESET_SELECTION)
    // Restore selection to existing focus cell
    selection.collapse(CellElement,0);
  else
    SelectedCellsType = newType;

  // Keep the same focus CellElement, just change the type
  DoCellSelection();
  SetSelectionButtons();

  // Note: globalCellElement should still be a clone of CellElement
}

function MoveSelection(forward)
{
  var newRowIndex = curRowIndex;
  var newColIndex = curColIndex;
  var focusCell;
  var inRow = false;

  if (SelectedCellsType == SELECT_ROW)
  {
    newRowIndex += (forward ? 1 : -1);

    // Wrap around if before first or after last row
    if (newRowIndex < 0)
      newRowIndex = lastRowIndex;
    else if (newRowIndex > lastRowIndex)
      newRowIndex = 0;
    inRow = true;

    // Use first cell in row for focus cell
    newColIndex = 0;
  }
  else
  {
    // Cell or column:
    if (!forward)
      newColIndex--;

    if (SelectedCellsType == SELECT_CELL)
    {
      // Skip to next cell
      if (forward)
        newColIndex += curColSpan;
    }
    else  // SELECT_COLUMN
    {
      // Use first cell in column for focus cell
      newRowIndex = 0;

      // Don't skip by colspan,
      //  but find first cell in next cellmap column
      if (forward)
        newColIndex++;
    }

    if (newColIndex < 0)
    {
      // Request is before the first cell in column

      // Wrap to last cell in column
      newColIndex = lastColIndex;

      if (SelectedCellsType == SELECT_CELL)
      {
        // If moving by cell, also wrap to previous...
        if (newRowIndex > 0)
          newRowIndex -= 1;
        else
          // ...or the last row
          newRowIndex = lastRowIndex;

        inRow = true;
      }
    }
    else if (newColIndex > lastColIndex)
    {
      // Request is after the last cell in column

      // Wrap to first cell in column
      newColIndex = 0;

      if (SelectedCellsType == SELECT_CELL)
      {
        // If moving by cell, also wrap to next...
        if (newRowIndex < lastRowIndex)
          newRowIndex++;
        else
          // ...or the first row
          newRowIndex = 0;

        inRow = true;
      }
    }
  }

  // Get the cell at the new location
  do {
    if (!GetCellData(newRowIndex, newColIndex))
    {
      dump("MoveSelection: CELL NOT FOUND\n");
      return;
    }
    if (inRow)
    {
      if (cellData.startRowIndex == newRowIndex)
        break;
      else
        // Cell spans from a row above, look for the next cell in row
        newRowIndex += cellData.actualRowSpan;
    }
    else
    {
      if (cellData.startColIndex == newColIndex)
        break;
      else
        // Cell spans from a Col above, look for the next cell in column
        newColIndex += cellData.actualColSpan;
    }
  }
  while(true);

  // Save data for current selection before changing
  if (CellDataChanged) // && dialog.ApplyBeforeMove.checked)
  {
    if (!ValidateCellData())
      return;

    editorShell.BeginBatchChanges();
    // Apply changes to all selected cells
    ApplyCellAttributes();
    editorShell.EndBatchChanges();

    SetCloseButton();
  }

  // Set cell and other data for new selection
  CellElement = cellData.cell;

  // Save globals for new current cell
  curRowIndex = cellData.startRowIndex;
  curColIndex = cellData.startColIndex;
  curColSpan = cellData.actualColSpan;

  // Copy for new global cell
  globalCellElement = CellElement.cloneNode(false);
  globalElement = globalCellElement;

  // Change the selection
  DoCellSelection();

  // Reinitialize dialog using new cell
//  if (!dialog.KeepCurrentData.checked)
  // Setting this false unchecks all "set attributes" checkboxes
  AdvancedEditUsed = false;
  InitCellPanel();
  AdvancedEditUsed = true;
}


function DoCellSelection()
{
  // Collapse selection into to the focus cell
  //  so editor uses that as start cell
  selection.collapse(CellElement, 0);

  switch (SelectedCellsType)
  {
    case SELECT_CELL:
      editorShell.SelectTableCell();
      break
    case SELECT_ROW:
      editorShell.SelectTableRow();
      break;
    default:
      editorShell.SelectTableColumn();
      break;
  }
  // Get number of cells selected
  var tagNameObj = new Object;
  var countObj = new Object;
  tagNameObj.value = "";
  var tableOrCellElement = editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);
  if (tagNameObj.value == "td")
    gSelectedCellCount = countObj.value;
  else
    gSelectedCellCount = 0;

  // Currently, we can only allow advanced editing on ONE cell element at a time
  //   else we ignore CSS, JS, and HTML attributes not already in dialog
  SetElementEnabledById("AdvancedEditButton2", gSelectedCellCount == 1);
}

function SetSelectionButtons()
{
  if (SelectedCellsType == SELECT_ROW)
  {
    // Trigger CSS to set images of up and down arrows
    dialog.PreviousButton.setAttribute("type","row");
    dialog.NextButton.setAttribute("type","row");
  }
  else
  {
    // or images of left and right arrows
    dialog.PreviousButton.setAttribute("type","col");
    dialog.NextButton.setAttribute("type","col");
  }
  DisableSelectionButtons((SelectedCellsType == SELECT_ROW && rowCount == 1) ||
                          (SelectedCellsType == SELECT_COLUMN && colCount == 1) ||
                          (rowCount == 1 && colCount == 1));
}

function DisableSelectionButtons( disable )
{
  dialog.PreviousButton.setAttribute("disabled", disable ? "true" : "false");
  dialog.NextButton.setAttribute("disabled", disable ? "true" : "false");
}

function SwitchToValidatePanel()
{
  if (currentPanel != validatePanel)
  {
    //Set index for starting panel on the <tabpanels> element
    TabPanels.setAttribute("index", validatePanel);
    if (validatePanel == CellPanel)
    {
      // Trigger setting of style for the tab widgets
      CellTab.setAttribute("selected", "true");
      TableTab.removeAttribute("selected");
    } else {
      TableTab.setAttribute("selected", "true");
      CellTab.removeAttribute("selected");
    }
    currentPanel = validatePanel;
  }
}

function SetAlign(listID, defaultValue, element, attName)
{
  var value = document.getElementById(listID).selectedItem.value;
  if (value == defaultValue)
    element.removeAttribute(attName);
  else
    element.setAttribute(attName, value);
}

function ValidateTableData()
{
  validatePanel = TablePanel;
  newRowCount = Number(ValidateNumber(dialog.TableRowsInput, null, 1, maxRows, null, null, true));
  if (gValidationError) return false;

  newColCount = Number(ValidateNumber(dialog.TableColumnsInput, null, 1, maxColumns, null, null, true));
  if (gValidationError) return false;

  // If user is deleting any cells, get confirmation
  // (This is a global to the dialog and we ask only once per dialog session)
  if ( !canDelete &&
        (newRowCount < rowCount ||
         newColCount < colCount) ) 
  {
    if (ConfirmWithTitle(GetString("DeleteTableTitle"), 
                         GetString("DeleteTableMsg"),
                         GetString("DeleteCells")) )
    {
      canDelete = true;
    }
    else
    {
      SetTextboxFocus(newRowCount < rowCount ? dialog.TableRowsInput : dialog.TableColumnsInput);
      return false;
    }
  }

  ValidateNumber(dialog.TableWidthInput, dialog.TableWidthUnits,
                 1, maxPixels, globalTableElement, "width");
  if (gValidationError) return false;

  var border = ValidateNumber(dialog.BorderWidthInput, null, 0, maxPixels, globalTableElement, "border");
  // TODO: Deal with "BORDER" without value issue
  if (gValidationError) return false;

  ValidateNumber(dialog.SpacingInput, null, 0, maxPixels, globalTableElement, "cellspacing");
  if (gValidationError) return false;

  ValidateNumber(dialog.PaddingInput, null, 0, maxPixels, globalTableElement, "cellpadding");
  if (gValidationError) return false;

  SetAlign("TableAlignList", defHAlign, globalTableElement, "align");

  // Color is set on globalCellElement immediately
  return true;
}

function ValidateCellData()
{

  validatePanel = CellPanel;

  if (dialog.CellHeightCheckbox.checked)
  {
    ValidateNumber(dialog.CellHeightInput, dialog.CellHeightUnits,
                    1, maxPixels, globalCellElement, "height");
    if (gValidationError) return false;
  }

  if (dialog.CellWidthCheckbox.checked)
  {
    ValidateNumber(dialog.CellWidthInput, dialog.CellWidthUnits,
                   1, maxPixels, globalCellElement, "width");
    if (gValidationError) return false;
  }

  if (dialog.CellHAlignCheckbox.checked)
  {
    var hAlign = dialog.CellHAlignList.selectedItem.value;

    // Horizontal alignment is complicated by "char" type
    // We don't change current values if user didn't edit alignment
    if (!alignWasChar)
    {
      globalCellElement.removeAttribute(charStr);

      // Always set "align" attribute,
      //  so the default "left" is effective in a cell
      //  when parent row has align set.
      globalCellElement.setAttribute("align", hAlign);
    }
  }

  if (dialog.CellVAlignCheckbox.checked)
  {
    // Always set valign (no default in 2nd param) so
    //  the default "middle" is effective in a cell
    //  when parent row has valign set.
    SetAlign("CellVAlignList", "", globalCellElement, "valign");
  }

  if (dialog.TextWrapCheckbox.checked)
  {
    if (dialog.TextWrapList.selectedIndex == 1)
      globalCellElement.setAttribute("nowrap","true");
    else
      globalCellElement.removeAttribute("nowrap");
  }

  return true;
}

function ValidateData()
{
  var result;
  var savePanel = currentPanel;

  // Validate current panel first
  if (currentPanel == TablePanel)
  {
    result = ValidateTableData();
    if (result)
      result = ValidateCellData();
  } else {
    result = ValidateCellData();
    if (result)
      result = ValidateTableData();
  }
  if(!result) return false;

  // If we passed, restore former currentPanel
  currentPanel = savePanel;

  // Set global element for AdvancedEdit
  if(currentPanel == TablePanel)
    globalElement = globalTableElement;
  else
    globalElement = globalCellElement;

  return true;
}

function ChangeCellTextbox(textboxID)
{
  // Filter input for just integers
  forceInteger(textboxID);

  if (currentPanel == CellPanel)
    CellDataChanged = true;
}

// Call this when a textbox or menulist is changed
//   so the checkbox is automatically set
function SetCheckbox(checkboxID)
{
  if (checkboxID && checkboxID.length > 0)
  {
    // Set associated checkbox
    document.getElementById(checkboxID).checked = true;
  }
  CellDataChanged = true;
}

function ChangeIntTextbox(textboxID, checkboxID)
{
  // Filter input for just integers
  forceInteger(textboxID);

  // Set associated checkbox
  SetCheckbox(checkboxID);
}

function CloneAttribute(destElement, srcElement, attr)
{
  var value = srcElement.getAttribute(attr);

  // Use editorShell methods since we are always
  //  modifying a table in the document and
  //  we need transaction system for undo
  if (!value || value.length == 0)
    editorShell.RemoveAttribute(destElement, attr);
  else
    editorShell.SetAttribute(destElement, attr, value);
}

function ApplyTableAttributes()
{
  var newAlign = dialog.TableCaptionList.selectedItem.value;
  if (!newAlign) newAlign = "";

  if (TableCaptionElement)
  {
    // Get current alignment
    var align = TableCaptionElement.align.toLowerCase();
    // This is the default
    if (!align) align = "top";

    if (newAlign == "")
    {
      // Remove existing caption
      editorShell.DeleteElement(TableCaptionElement);
      TableCaptionElement = null;
    }
    else if( align != newAlign)
    {
      if (align == "top") // This is default, so don't explicitly set it
        editorShell.RemoveAttribute(TableCaptionElement, "align");
      else
        editorShell.SetAttribute(TableCaptionElement, "align", newAlign);
    }
  }
  else if (newAlign != "")
  {
    // Create and insert a caption:
    TableCaptionElement = editorShell.CreateElementWithDefaults("caption");
    if (TableCaptionElement)
    {
      if (newAlign != "top")
        TableCaptionElement.setAttribute("align", newAlign);

      // Insert it into the table - caption is always inserted as first child
      editorShell.InsertElement(TableCaptionElement, TableElement, 0, true);

      // Put selecton back where it was
      ChangeSelection(RESET_SELECTION);
    }
  }

  var countDelta;
  var foundcell;
  var i;

  if (newRowCount != rowCount)
  {
    countDelta = newRowCount - rowCount;
    if (newRowCount > rowCount)
    {
      // Append new rows
      // Find first cell in last row
      if(GetCellData(lastRowIndex, 0))
      {
        try {
          // Move selection to the last cell
          selection.collapse(cellData.cell,0);
          // Insert new rows after it
          editorShell.InsertTableRow(countDelta, true);
          rowCount = newRowCount;
          lastRowIndex = rowCount - 1;
          // Put selecton back where it was
          ChangeSelection(RESET_SELECTION);
        }
        catch(ex) {
          dump("FAILED TO FIND FIRST CELL IN LAST ROW\n");
        }
      }
    }
    else
    {
      // Delete rows
      if (canDelete)
      {
        // Find first cell starting in first row we delete
        var firstDeleteRow = rowCount + countDelta;
        foundCell = false;
        for ( i = 0; i <= lastColIndex; i++)
        {
          if (!GetCellData(firstDeleteRow, i))
            break; // We failed to find a cell

          if (cellData.startRowIndex == firstDeleteRow)
          {
            foundCell = true;
            break;
          }
        };
        if (foundCell)
        {
          try {
            // Move selection to the cell we found
            selection.collapse(cellData.cell, 0);
            editorShell.DeleteTableRow(-countDelta);
            rowCount = newRowCount;
            lastRowIndex = rowCount - 1;
            if (curRowIndex > lastRowIndex)
              // We are deleting our selection
              // move it to start of table
              ChangeSelectionToFirstCell()
            else
              // Put selecton back where it was
              ChangeSelection(RESET_SELECTION);
          }
          catch(ex) {
            dump("FAILED TO FIND FIRST CELL IN LAST ROW\n");
          }
        }
      }
    }
  }

  if (newColCount != colCount)
  {
    countDelta = newColCount - colCount;

    if (newColCount > colCount)
    {
      // Append new columns
      // Find last cell in first column
      if(GetCellData(0, lastColIndex))
      {
        try {
          // Move selection to the last cell
          selection.collapse(cellData.cell,0);
          editorShell.InsertTableColumn(countDelta, true);
          colCount = newColCount;
          lastColIndex = colCount-1;
          // Restore selection
          ChangeSelection(RESET_SELECTION);
        }
        catch(ex) {
          dump("FAILED TO FIND FIRST CELL IN LAST COLUMN\n");
        }
      }
    }
    else
    {
      // Delete columns
      if (canDelete)
      {
        var firstDeleteCol = colCount + countDelta;
        foundCell = false;
        for ( i = 0; i <= lastRowIndex; i++)
        {
          // Find first cell starting in first column we delete
          if (!GetCellData(i, firstDeleteCol))
            break; // We failed to find a cell

          if (cellData.startColIndex == firstDeleteCol)
          {
            foundCell = true;
            break;
          }
        };
        if (foundCell)
        {
          try {
            // Move selection to the cell we found
            selection.collapse(cellData.cell, 0);
            editorShell.DeleteTableColumn(-countDelta);
            colCount = newColCount;
            lastColIndex = colCount-1;
            if (curColIndex > lastColIndex)
              ChangeSelectionToFirstCell()
            else
              ChangeSelection(RESET_SELECTION);
          }
          catch(ex) {
            dump("FAILED TO FIND FIRST CELL IN LAST ROW\n");
          }
        }
      }
    }
  }

  // Clone all remaining attributes to pick up
  //  anything changed by Advanced Edit Dialog
  editorShell.CloneAttributes(TableElement, globalTableElement);
}

function ApplyCellAttributes()
{
  var selectedCell = editorShell.GetFirstSelectedCell();
  if (!selectedCell)
    return;

  if (gSelectedCellCount == 1)
  {
    // When only one cell is selected, simply clone entire element,
    //  thus CSS and JS from Advanced edit is copied
    editorShell.CloneAttributes(selectedCell, globalCellElement);

    if (dialog.CellStyleCheckbox.checked)
    {
      var currentStyleIndex = (selectedCell.nodeName.toLowerCase() == "th") ? 1 : 0;
      if (dialog.CellStyleList.selectedIndex != currentStyleIndex)
      {
        // Switch cell types
        // (replaces with new cell and copies attributes and contents)
        selectedCell = editorShell.SwitchTableCellHeaderType(selectedCell);
      }
    }
  }
  else
  {
    // Apply changes to all selected cells
    //XXX THIS DOESN'T COPY ADVANCED EDIT CHANGES!
    while (selectedCell)
    {
      ApplyAttributesToOneCell(selectedCell);
      selectedCell = editorShell.GetNextSelectedCell();
    }
  }
  CellDataChanged = false;
}

function ApplyAttributesToOneCell(destElement)
{
  if (dialog.CellHeightCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "height");

  if (dialog.CellWidthCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "width");

  if (dialog.CellHAlignCheckbox.checked)
  {
    CloneAttribute(destElement, globalCellElement, "align");
    CloneAttribute(destElement, globalCellElement, charStr);
  }

  if (dialog.CellVAlignCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "valign");

  if (dialog.TextWrapCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "nowrap");

  if (dialog.CellStyleCheckbox.checked)
  {
    var newStyleIndex = dialog.CellStyleList.selectedIndex;
    var currentStyleIndex = (destElement.nodeName.toLowerCase() == "th") ? 1 : 0;

    if (newStyleIndex != currentStyleIndex)
    {
      // Switch cell types
      // (replaces with new cell and copies attributes and contents)
      destElement = editorShell.SwitchTableCellHeaderType(destElement);
    }
  }

  if (dialog.CellColorCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "bgcolor");
}

function SetCloseButton()
{
  // Change text on "Cancel" button after Apply is used
  if (!ApplyUsed)
  {
    document.getElementById("cancel").setAttribute("label",GetString("Close"));
    ApplyUsed = true;
  }
}

function onApply()
{
  Apply();
  return false; // don't close window
}

function Apply()
{
  if (ValidateData())
  {
    editorShell.BeginBatchChanges();

    ApplyTableAttributes();

    // We may have just a table, so check for cell element
    if (globalCellElement)
      ApplyCellAttributes();

    editorShell.EndBatchChanges();

    SetCloseButton();
    return true;
  }
  return false;
}

function doHelpButton()
{
  openHelp("chrome://help/content/help.xul?table_properties");
}

function onOK()
{
  // Do same as Apply and close window if ValidateData succeeded
  var retVal = Apply();
  if (retVal)
    SaveWindowLocation();

  return retVal;
}
