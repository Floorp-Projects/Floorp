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
var TabPanel;
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

var selectedCellCount = 0;
var error = 0;
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
    window.close();

  // Get dialog widgets - Table Panel
  dialog.TableRowsInput = document.getElementById("TableRowsInput");
  dialog.TableColumnsInput = document.getElementById("TableColumnsInput");
  dialog.TableHeightInput = document.getElementById("TableHeightInput");
  dialog.TableHeightUnits = document.getElementById("TableHeightUnits");
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
  dialog.ApplyBeforeMove =  document.getElementById("ApplyBeforeMove");
  // Currently, we always load attributes when changing selection
  // (Let's keep this for possible future use)
  //dialog.KeepCurrentData = document.getElementById("KeepCurrentData");

  dialog.CellHeightInput = document.getElementById("CellHeightInput");
  dialog.CellHeightUnits = document.getElementById("CellHeightUnits");
  dialog.CellWidthInput = document.getElementById("CellWidthInput");
  dialog.CellWidthUnits = document.getElementById("CellWidthUnits");
  dialog.RowSpanInput = document.getElementById("RowSpanInput");
  dialog.ColSpanInput = document.getElementById("ColSpanInput");
  dialog.CellHAlignList = document.getElementById("CellHAlignList");
  dialog.CellAlignCharInput = document.getElementById("CellAlignCharInput");
  dialog.CellVAlignList = document.getElementById("CellVAlignList");
  dialog.CellInheritColor = document.getElementById("CellInheritColor");
  dialog.HeaderCheck = document.getElementById("HeaderCheck");
  dialog.NoWrapCheck = document.getElementById("NoWrapCheck");

  TabPanel = document.getElementById("TabPanel");
  var TableTab = document.getElementById("TableTab");
  var CellTab = document.getElementById("CellTab");

  TableElement = editorShell.GetElementOrParentByTagName("table", null);
  if(!TableElement)
  {
    dump("Failed to get table element!\n");
    window.close();
  }
  globalTableElement = TableElement.cloneNode(false);

  var tagNameObj = new Object;
  var countObj = new Object;
  var tableOrCellElement = editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);
  selectedCellCount = countObj.value;

  if (tagNameObj.value == "td")
  {
    // We are in a cell
    CellElement = tableOrCellElement;
    globalCellElement = CellElement.cloneNode(false);

    // Tells us whether cell, row, or column is selected
    SelectedCellsType = editorShell.GetSelectedCellsType(TableElement);
    SetSpanEnable();
    
    // Ignore types except Cell, Row, and Column
    if (SelectedCellsType < SELECT_CELL || SelectedCellsType > SELECT_COLUMN)
      SelectedCellsType = SELECT_CELL;

    // Be sure at least 1 cell is selected.
    // (If the count is 0, then we were inside the cell.)
    if (selectedCellCount == 0)
      editorShell.SelectTableCell();

    // Get location in the cell map
    curRowIndex = editorShell.GetRowIndex(CellElement);
    curColIndex = editorShell.GetColumnIndex(CellElement);

    // We save the current colspan to quickly 
    //  move selection from from cell to cell
    if (GetCellData(curRowIndex, curColIndex))
      curColSpan = cellData.colSpan;

    // Set appropriate icons in the Previous/Next buttons
    SetSelectionButtons();

    // Starting TabPanel name is passed in
    if (window.arguments[1] == "CellPanel") 
    {
      currentPanel = CellPanel;

      //Set index for starting panel on the <tabpanel> element
      TabPanel.setAttribute("index", CellPanel);
  
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
      CellTab.setAttribute("disabled", "true");
  }

  doSetOKCancel(onOK, onCancel);

  // Note: we must use TableElement, not globalTableElement for these,
  //  thus we should not put this in InitDialog.
  // Instead, monitor desired counts with separate globals
  rowCount = editorShell.GetTableRowCount(TableElement);
  lastRowIndex = rowCount-1;
  colCount = editorShell.GetTableColumnCount(TableElement);
  lastColIndex = colCount-1;

  // User can change these via textfields  
  newRowCount = rowCount;
  newColCount = colCount;

  InitDialog();

  if (currentPanel == CellPanel)
    dialog.SelectionList.focus(); 
  else
    SetTextfieldFocus(dialog.TableRowsInput);

  SetWindowLocation();
}


function InitDialog()
{
  // Get Table attributes
  dialog.TableRowsInput.value = rowCount;
  dialog.TableColumnsInput.value = colCount;
  dialog.TableHeightInput.value = InitPixelOrPercentMenulist(globalTableElement, TableElement, "height", "TableHeightUnits", gPercent);
  dialog.TableWidthInput.value = InitPixelOrPercentMenulist(globalTableElement, TableElement, "width", "TableWidthUnits", gPercent);
  dialog.BorderWidthInput.value = globalTableElement.border;
  dialog.SpacingInput.value = globalTableElement.cellSpacing;
  dialog.PaddingInput.value = globalTableElement.cellPadding;

  //BUG: The align strings are converted: e.g., "center" becomes "Center";
  var halign = globalTableElement.align.toLowerCase();
  if (halign == centerStr)
    dialog.TableAlignList.selectedIndex = 1;
  else if (halign == bottomStr)
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

  InitCellPanel(SelectedCellsType);
}

function InitCellPanel()
{
  // Get cell attributes
  if (globalCellElement)
  {
    // This assumes order of items is Cell, Row, Column
    dialog.SelectionList.selectedIndex = SelectedCellsType-1;

    dialog.CellHeightInput.value = InitPixelOrPercentMenulist(globalCellElement, CellElement, "height", "CellHeightUnits", gPercent);
    dialog.CellWidthInput.value = InitPixelOrPercentMenulist(globalCellElement, CellElement, "width", "CellWidthUnits", gPixel);

    dialog.RowSpanInput.value = globalCellElement.getAttribute("rowspan");
    dialog.ColSpanInput.value = globalCellElement.getAttribute("colspan");
    
    var valign = globalCellElement.vAlign.toLowerCase();
    if (valign == topStr)
      dialog.CellVAlignList.selectedIndex = 0;
    else if (valign == bottomStr)
      dialog.CellVAlignList.selectedIndex = 2;
    else // Default = middle
      dialog.CellVAlignList.selectedIndex = 1;
    
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
        var alignChar = globalCellElement.getAttribute(charStr);
        if (alignChar && alignChar.length == 1)
        {
          dialog.CellAlignCharInput.value = alignChar;
          dialog.CellHAlignList.selectedIndex = 4;
        } 
        else
        {
          // "char" align set but no alignment char value
          dialog.CellHAlignList.selectedIndex = 0;
        }
        break;
      default:  // left
        dialog.CellHAlignList.selectedIndex = 0;
        break;
    }

    // Hide align char input if not appropriate align type
    dialog.CellHAlignList.selectedIndex == 4 ? "true" : "";

    dialog.HeaderCheck.checked = globalCellElement.nodeName.toLowerCase() == "th";
    dialog.NoWrapCheck.checked = globalCellElement.noWrap ? true : false;
    
    CellColor = globalCellElement.bgColor;
    SetColor("cellBackgroundCW", CellColor); 
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
  if (dialog.CellHAlignList.selectedIndex == 4)
  {
    // Activate the textfield for the alignment character
    dialog.CellAlignCharInput.removeAttribute("collapsed");
    SetTextfieldFocus(dialog.CellAlignCharInput);
  }
  else
    dialog.CellAlignCharInput.setAttribute("collapsed","true");
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
  }    
  CellDataChanged = true;

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

  // Enable/Disable appropriate span textfields
  SetSpanEnable();

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

  // Set cell and other data
  CellElement = cellData.cell;

  globalTableCell = CellElement.cloneNode(false);
  globalElement = globalCellElement;

  // Save globals for current cell
  curRowIndex = cellData.startRowIndex;
  curColIndex = cellData.startColIndex;
  curColSpan = cellData.actualColSpan;

  if (CellDataChanged && dialog.ApplyBeforeMove.checked)
  {
    if (!ValidateCellData())
      return;

    editorShell.BeginBatchChanges();
    // Apply changes to all selected cells
    ApplyCellAttributes();
    editorShell.EndBatchChanges();

    SetCloseButton();
  }

  // Reinitialize dialog using new cell
//  if (!dialog.KeepCurrentData.checked)
  InitCellPanel();

  // Change the selection
  DoCellSelection();
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
}

function SetSpanEnable()
{
  // If entire row is selected, don't allow changing colspan...
  dialog.RowSpanInput.setAttribute("disabled", (SelectedCellsType == SELECT_COLUMN) ? "true" : "false");
  // ...and similarly:
  dialog.ColSpanInput.setAttribute("disabled", (SelectedCellsType == SELECT_ROW) ? "true" : "false");
}

function SwitchPanel(newPanel)
{
  if (currentPanel != newPanel)
  {
    //Set index for starting panel on the <tabpanel> element
    TabPanel.setAttribute("index", newPanel);
    if (newPanel == CellPanel)
    {    
      // Trigger setting of style for the tab widgets
      CellTab.setAttribute("selected", "true");
      TableTab.removeAttribute("selected");
    } else {
      TableTab.setAttribute("selected", "true");
      CellTab.removeAttribute("selected");
    }
    currentPanel = newPanel;
  }
}

// More weirdness: Can't pass in "inputWidget" -- becomes undefined?!
// Must pass in just ID and use getElementById
function ValidateNumber(inputWidgetID, listWidget, minVal, maxVal, element, attName)
{
  var inputWidget = document.getElementById(inputWidgetID);
  // Global error return value
  error = false;
  var maxLimit = maxVal;
  var isPercent = false;

  var numString = inputWidget.value.trimString();
  if (numString)
  {
    if (listWidget)
      isPercent = (listWidget.selectedIndex == 1);
    if (isPercent)
      maxLimit = 100;

    numString = ValidateNumberString(numString, minVal, maxLimit);
    if(!numString)
    {
      dump("Error returned from ValidateNumberString\n");

      // Switch to appropriate panel for error reporting
      SwitchPanel(validatePanel);

      // Error - shift to offending input widget
      SetTextfieldFocus(inputWidget);
      error = true;
    }
    else      
    {
      if (isPercent)
        numString += "%";
      if (element)
        element.setAttribute(attName, numString);
    }
  } else if (element) {
    element.removeAttribute(attName);  
  }
  return numString;
}

function SetAlign(listID, defaultValue, element, attName)
{
  var value = document.getElementById(listID).selectedItem.data;
  if (value == defaultValue)
    element.removeAttribute(attName);
  else
    element.setAttribute(attName, value);
}

function ValidateTableData()
{
  validatePanel = TablePanel;
  newRowCount = Number(ValidateNumber("TableRowsInput", null, 1, maxRows, null, null));
  if (error) return false;

  newColCount = Number(ValidateNumber("TableColumnsInput", null, 1, maxColumns, null, null));
  if (error) return false;


  ValidateNumber("TableHeightInput", dialog.TableHeightUnits, 
                  1, maxPixels, globalTableElement, "height");
  if (error) return false;


  ValidateNumber("TableWidthInput", dialog.TableWidthUnits, 
                 1, maxPixels, globalTableElement, "width");
  if (error) return false;

  var border = ValidateNumber("BorderWidthInput", null, 0, maxPixels, globalTableElement, "border");
  // TODO: Deal with "BORDER" without value issue
  if (error) return false;

  ValidateNumber("SpacingInput", null, 0, maxPixels, globalTableElement, "cellspacing");
  if (error) return false;

  ValidateNumber("PaddingInput", null, 0, maxPixels, globalTableElement, "cellpadding");
  if (error) return false;

  SetAlign("TableAlignList", defHAlign, globalTableElement, "align");

  // Color is set on globalCellElement immediately
  return true;
}

function ValidateCellData()
{

  validatePanel = CellPanel;

  ValidateNumber("CellHeightInput", dialog.CellHeightUnits, 
                  1, maxPixels, globalCellElement, "height");
  if (error) return false;

  ValidateNumber("CellWidthInput", dialog.CellWidthUnits, 
                 1, maxPixels, globalCellElement, "width");
  if (error) return false;
  
  if (dialog.RowSpanInput.disabled != "true")
  {
    // Note that span = 0 is allowed and means "span entire row/col"
    ValidateNumber("RowSpanInput", null,
                   0, rowCount, globalCellElement, "rowspan");
    if (error) return false;
  }
  if (dialog.ColSpanInput.getAttribute("disabled") != "true")
  {
    ValidateNumber("ColSpanInput", null,
                   0, colCount, globalCellElement, "colspan");
    if (error) return false;
  }

  // Vertical alignment is complicated by "char" type
  var hAlign = dialog.CellHAlignList.selectedItem.data;

  if (hAlign != charStr)
    globalCellElement.removeAttribute(charStr);
  
  if (hAlign == "left")
  {
    globalCellElement.removeAttribute("align");
  }
  else
  {
    if (hAlign == charStr)
    {
      //Note: Is space a valid align character?
      // Assume yes and don't use "trimString()"
      var alignChar = dialog.CellAlignCharInput.value.charAt(0);
      globalCellElement.setAttribute(charStr, alignChar);
      if (!alignChar)
      {
        ShowInputErrorMessage(GetString("NoAlignChar"));
        SetTextfieldFocus(dialog.CellAlignCharInput);
        return false;
      }
    }
    globalCellElement.setAttribute("align", hAlign);
  }

  SetAlign("CellVAlignList", defVAlign, globalCellElement, "valign");

  if (dialog.NoWrapCheck.checked)
    globalCellElement.setAttribute("nowrap","true");
  else
    globalCellElement.removeAttribute("nowrap");

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

function ChangeCellTextfield(textfieldID)
{
  // Filter input for just integers
  forceInteger(textfieldID);

  if (currentPanel == CellPanel)
    CellDataChanged = true;
}

function ChangeCellData()
{
  CellDataChanged = true;
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

function ConfirmDeleteCells()
{
  if (0 == editorShell.ConfirmWithTitle(GetString("DeleteTableTitle"), GetString("DeleteTableMsg"),
                                        GetString("DeleteCells"), ""))
  {
    return true;
  }
  return false;
}

function ApplyTableAttributes()
{
  var newAlign = dialog.TableCaptionList.selectedItem.data;
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
      editorShell.InsertElement(TableCaptionElement, TableElement, 0);

      // Put selecton back where it was
      ChangeSelection(RESET_SELECTION);
    }
  }

  var countDelta;
  var foundcell;
  var i;

  // If user is deleting any cells and get confirmation
  // (This is a global to the dialog and we ask only once per dialog session)
  if ( !canDelete &&
       (newRowCount < rowCount ||
        newColCount < colCount) &&
       ConfirmDeleteCells() )
  {
    canDelete = true;
  }

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

  CloneAttribute(TableElement, globalTableElement, "height");
  CloneAttribute(TableElement, globalTableElement, "width");
  CloneAttribute(TableElement, globalTableElement, "border");
  CloneAttribute(TableElement, globalTableElement, "cellspacing");
  CloneAttribute(TableElement, globalTableElement, "cellpadding");
  CloneAttribute(TableElement, globalTableElement, "align");
  CloneAttribute(TableElement, globalTableElement, "bgcolor");
}

function ApplyCellAttributes()
{
  // Apply changes to all selected cells
  var selectedCell = editorShell.GetFirstSelectedCell();
  while (selectedCell)
  {
    ApplyAttributesToOneCell(selectedCell); 
    selectedCell = editorShell.GetNextSelectedCell();
  }
  CellDataChanged = false;
}

function ApplyAttributesToOneCell(destElement)
{
  CloneAttribute(destElement, globalCellElement, "height");

  CloneAttribute(destElement, globalCellElement, "width");

  if (dialog.RowSpanInput.getAttribute("disabled") != "true")
   CloneAttribute(destElement, globalCellElement, "rowspan");

  if (dialog.ColSpanInput.getAttribute("disabled") != "true")
    CloneAttribute(destElement, globalCellElement, "colspan");

  CloneAttribute(destElement, globalCellElement, "align");
  CloneAttribute(destElement, globalCellElement, charStr);
  CloneAttribute(destElement, globalCellElement, "valign");
  CloneAttribute(destElement, globalCellElement, "nowrap");
  CloneAttribute(destElement, globalCellElement, "bgcolor");

  if (dialog.HeaderCheck.checked == (destElement.nodeName.toLowerCase() == "td"))
  {
    // Switch cell types 
    // (replaces with new cell and copies attributes and contents)
    destElement = editorShell.SwitchTableCellHeaderType(destElement);
  }
}

function SetCloseButton()
{
  // Change text on "Cancel" button after Apply is used
  if (!ApplyUsed)
  {
    document.getElementById("cancel").setAttribute("value",GetString("Close"));
    ApplyUsed = true;
  }
}

function Apply()
{
  if (ValidateData())
  {
    editorShell.BeginBatchChanges();

    // Can't use editorShell.CloneAttributes -- we must change only
    //   attributes that are checked!
    ApplyTableAttributes();

    // We may have just a table, so check for cell element
    if (globalCellElement)
    {
      ApplyCellAttributes();
      // Be sure user didn't mess up table by setting incorrect span values
      editorShell.NormalizeTable(TableElement);
    }

    editorShell.EndBatchChanges();

    SetCloseButton();
    return true;
  }
  return false;
}

function onOK()
{
  // Do same as Apply and close window if ValidateData succeeded
  var retVal = Apply();
  if (retVal)
    SaveWindowLocation();

  return retVal;
}
