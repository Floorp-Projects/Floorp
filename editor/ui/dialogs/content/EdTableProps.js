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
var TabPanel;
var dialog;
var globalCellElement;
var globalTableElement
var captionElement;
var TablePanel = 0;
var CellPanel = 1;
var currentPanel = TablePanel;
var validatePanel;
var defHAlignIndex = 0; // = left, index=0
var charIndex = 4;
var centerStr =   "center";  //Index=1
var rightStr =    "right";   // 2
var justifyStr =  "justify"; // 3
var charStr =     "char";    // 4
var defVAlignIndex = 1; // = middle
var topStr =      "top";
var bottomStr =   "bottom";

var bgcolor = "bgcolor";
var CellIsHeader = false;
var rowCount = 1;
var colCount = 1;
var selectedCellCount = 0;
var error = 0;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  dialog = new Object;
  if (!dialog)
  {
    dump("Failed to create dialog object!!!\n");
    window.close();
  }
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
  dialog.TableAlignSelect = document.getElementById("TableAlignSelect");
  dialog.CaptionSelect = document.getElementById("CaptionSelect");
  dialog.TableInheritColor = document.getElementById("TableInheritColor");
  dialog.TableImageInput = document.getElementById("TableImageInput");
  dialog.TableImageButton = document.getElementById("TableImageButton");
//  dialog.TableLeaveLocCheck = document.getElementById("TableLeaveLocCheck");

  // Cell Panel
  dialog.SelectionList = document.getElementById("SelectionList");
  dialog.SelectCellItem = document.getElementById("SelectCellItem");
  dialog.SelectRowItem = document.getElementById("SelectRowItem");
  dialog.SelectColumnItem = document.getElementById("SelectColumnItem");

  dialog.CellHeightInput = document.getElementById("CellHeightInput");
  dialog.CellHeightUnits = document.getElementById("CellHeightUnits");
  dialog.CellWidthInput = document.getElementById("CellWidthInput");
  dialog.CellWidthUnits = document.getElementById("CellWidthUnits");
  dialog.RowSpanInput = document.getElementById("RowSpanInput");
  dialog.ColSpanInput = document.getElementById("ColSpanInput");
  dialog.CellHAlignSelect = document.getElementById("CellHAlignSelect");
  dialog.CellAlignCharInput = document.getElementById("CellAlignCharInput");
  dialog.CellVAlignSelect = document.getElementById("CellVAlignSelect");
  dialog.HeaderCheck = document.getElementById("HeaderCheck");
  dialog.NoWrapCheck = document.getElementById("NoWrapCheck");
  dialog.CellInheritColor = document.getElementById("CellInheritColor");
  dialog.CellImageInput = document.getElementById("CellImageInput");
  dialog.CellImageButton = document.getElementById("CellImageButton");
//  dialog.CellLeaveLocCheck = document.getElementById("CellLeaveLocCheck");

  TableElement = editorShell.GetElementOrParentByTagName("table", null);
  var tagNameObj = new Object;
  var countObj = new Object;
  var element = editorShell.GetSelectedOrParentTableElement(tagNameObj, countObj);
  var tagName = tagNameObj.value;
  selectedCellCount = countObj.value;

  if (tagNameObj.value == "td")
  {
dump("Cell is selected or is selection parent. Selected Cell count = "+selectedCellCount+"\n");
    CellElement = element;

    // Be sure at least 1 cell is selected.
    // If the count is 0, then a cell we are inside the cell.
    if (selectedCellCount == 0)
      editorShell.SelectTableCell();
  }


  if(!TableElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
  // We allow a missing cell -- see below

  TabPanel = document.getElementById("TabPanel");
  var TableTab = document.getElementById("TableTab");
  var CellTab = document.getElementById("CellTab");
  
  
  // Starting TabPanel name is passed in
  if (window.arguments[1] == "CellPanel") currentPanel = CellPanel;

  globalTableElement = TableElement.cloneNode(false);
  if (CellElement)
    globalCellElement = CellElement.cloneNode(false);

  // Activate the Cell Panel if requested
  if (currentPanel == CellPanel)
  {
    // We must have a cell element to start in this panel
    if(!CellElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
    }

    //Set index for starting panel on the <tabpanel> element
    TabPanel.setAttribute("index", CellPanel);
    
    // Trigger setting of style for the tab widgets
    CellTab.setAttribute("selected", "true");
    TableTab.removeAttribute("selected");

    // Start editing on the cell element
    globalElement = globalCellElement;
  }
  else
  {
    currentPanel = TablePanel;

    // Start editing on the table element
    globalElement = globalTableElement;
  }

  if(!CellElement)
  {
    // Disable the Cell Properties tab -- only allow table props
    CellTab.setAttribute("disabled", "true");
  }
  
  doSetOKCancel(onOK, null);

  // Note: we must use TableElement, not globalTableElement for these,
  //  thus we should not put this in InitDialog.
  // Instead, monitor desired counts with separate globals
  rowCount = editorShell.GetTableRowCount(TableElement);
  colCount = editorShell.GetTableColumnCount(TableElement);

  // This uses values set on global Elements;
  InitDialog();

  // Should be dialog.TableRowsInput, or
  //  SelectionList in cel panenl,
  // but this is disabled for Beta1disabled for Beta1
  if (currentPanel == CellPanel)
    dialog.CellHeightInput.focus(); 
  else
    dialog.TableHeightInput.focus();
}


function InitDialog()
{
  // Get Table attributes
  dialog.TableRowsInput.value = rowCount;
  dialog.TableColumnsInput.value = colCount;
  dialog.TableHeightInput.value = InitPixelOrPercentMenulist(globalTableElement, TableElement, "height", "TableHeightUnits");
  dialog.TableWidthInput.value = InitPixelOrPercentMenulist(globalTableElement, TableElement, "width", "TableWidthUnits");
  dialog.BorderWidthInput.value = globalTableElement.border;
  dialog.SpacingInput.value = globalTableElement.cellSpacing;
  dialog.PaddingInput.value = globalTableElement.cellPadding;

  //BUG: The align strings are converted: e.g., "center" becomes "Center";
  var halign = globalTableElement.align.toLowerCase();
  if (halign == centerStr)
    dialog.TableAlignSelect.selectedIndex = 1;
  else if (halign == bottomStr)
    dialog.TableAlignSelect.selectedIndex = 2;
  else // Default = left
    dialog.TableAlignSelect.selectedIndex = 0;
  
  captionElement = globalTableElement.caption;
dump("Caption Element = "+captionElement+"\n");
  var index = 0;
  if (captionElement)
  {
    if(captionElement.vAlign == "top")
      index = 1;
    else
      index = 2;
  }
  dialog.CaptionSelect.selectedIndex = index;
  
  if (globalTableElement.background)
    dialog.TableImageInput.value = globalTableElement.background;
  
  SetColor("tableBackgroundCW", globalTableElement.bgColor); 

  // Get cell attributes
  if (globalCellElement)
  {
    // Test if entire rows or columns are selected and set menu appropriately
    var SelectionType = editorShell.GetSelectedCellsType(TableElement);
    dump("SelectionList.selectedIndex = "+dialog.SelectionList.selectedIndex+"\n");
    switch (SelectionType)
    {
      case 2:
        dialog.SelectionList.selectedItem = dialog.SelectRowItem;
        break;
      case 3:
        dialog.SelectionList.selectedItem = dialog.SelectColumnItem;
        break;
      default:
        dialog.SelectionList.selectedItem = dialog.SelectCellItem;
        break;
    }
    dialog.CellHeightInput.value = InitPixelOrPercentMenulist(globalCellElement, CellElement, "height", "CellHeightUnits");
    dialog.CellWidthInput.value = InitPixelOrPercentMenulist(globalCellElement, CellElement, "width", "CellWidthUnits");

//BUG: We don't support "rowSpan" or "colSpan" JS attributes?
dump("RowSpan="+globalCellElement.rowSpan+" ColSpan="+globalCellElement.colSpan+"\n");

    dialog.RowSpanInput.value = globalCellElement.getAttribute("rowspan");
    dialog.ColSpanInput.value = globalCellElement.getAttribute("colspan");
    
    var valign = globalCellElement.vAlign.toLowerCase();
    if (valign == topStr)
      dialog.CellVAlignSelect.selectedIndex = 0;
    else if (valign == bottomStr)
      dialog.CellVAlignSelect.selectedIndex = 2;
    else // Default = middle
      dialog.CellVAlignSelect.selectedIndex = 1;
    
    var halign = globalCellElement.align.toLowerCase();
    switch (halign)
    {
      case centerStr:
        dialog.CellHAlignSelect.selectedIndex = 1;
        break;
      case rightStr:
        dialog.CellHAlignSelect.selectedIndex = 2;
        break;
      case justifyStr:
        dialog.CellHAlignSelect.selectedIndex = 3;
        break;
      case charStr:
        var alignChar = globalCellElement.getAttribute(charStr);
        if (alignChar && alignChar.length == 1)
        {
          dialog.CellAlignCharInput.value = alignChar;
          dialog.CellHAlignSelect.selectedIndex = 4;
        } else {
          // "char" align set but no alignment char value
          dialog.CellHAlignSelect.selectedIndex = 0;
        }
        break;
      default:  // left
        dialog.CellHAlignSelect.selectedIndex = 0;
        break;
    }
    // Show/hide extra message to explain "default" case
    SelectCellHAlign();

    CellIsHeader = (globalCellElement.nodeName == "TH");
    dialog.HeaderCheck.checked = CellIsHeader;
    dialog.NoWrapCheck.checked = globalCellElement.noWrap;
    
    SetColor("cellBackgroundCW", globalCellElement.bgColor); 
    
    if (globalCellElement.background)
      dialog.CellImageInput.value = globalCellElement.background;
  }
}

//TODO: Should we validate the panel before leaving it? We don't now
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
  dump("dialog.CellHAlignSelect.selectedIndex ="+dialog.CellHAlignSelect.selectedIndex+"\n");
  if (dialog.CellHAlignSelect.selectedIndex == 4)
    dialog.CellAlignCharInput.removeAttribute("visibility");
  else
    dialog.CellAlignCharInput.setAttribute("visibility","hidden");
}

function GetColorAndUpdate(ColorPickerID, ColorWellID, widget)
{
  // Close the colorpicker
  widget.parentNode.closePopup();
  var color = null;
  if (ColorPickerID)
    color = getColor(ColorPickerID);

  SetColor(ColorWellID, color);
}

function SetColor(ColorWellID, color)
{
  // Save the color
  if (ColorWellID == "cellBackgroundCW")
  {
    if (color)
    {
      globalCellElement.setAttribute(bgcolor, color);
      dialog.CellInheritColor.setAttribute("hidden","true");
    }
    else
    {
      globalCellElement.removeAttribute(bgcolor);
      // Reveal addition message explaining "default" color
      dialog.CellInheritColor.removeAttribute("hidden");
    }
  }
  else
  {
    if (color)
    {
      globalTableElement.setAttribute(bgcolor, color);
      dialog.TableInheritColor.setAttribute("hidden","true");
    }
    else
    {
      globalTableElement.removeAttribute(bgcolor);
      dialog.TableInheritColor.removeAttribute("hidden");
    }
  }    
  setColorWell(ColorWellID, color); 
}

function SelectPrevious()
{
  //TODO:Implement me!
}

function SelectPrevious()
{
  //TODO:Implement me!
}

function ChooseTableImage()
{
  // Get a local file, converted into URL format
  fileName = GetLocalFileURL("img");
  if (fileName && fileName.length > 0) {
    dialog.TableImageInput.setAttribute("value",fileName);
  }
  // Put focus into the input field
  dialog.TableImageInput.focus();
}

function ChooseCellImage()
{
  fileName = GetLocalFileURL("img");
  if (fileName && fileName.length > 0) {
    dialog.CellImageInput.setAttribute("value",fileName);
  }
  // Put focus into the input field
  dialog.CellImageInput.focus();
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
function ValidateNumber(inputWidgetID, selectWidget, minVal, maxVal, element, attName)
{
  var inputWidget = document.getElementById(inputWidgetID);
  // Global error return value
  error = false;
  var maxLimit = maxVal;
  var isPercent = false;

  var numString = inputWidget.value.trimString();
  if (numString && numString != "")
  {
    if (selectWidget)
      isPercent = (selectWidget.selectedIndex == 1);
    if (isPercent)
      maxLimit = 100;

    numString = ValidateNumberString(numString, minVal, maxLimit);
    if(numString == "")
    {
      dump("Error returned from ValidateNumberString\n");

      // Switch to appropriate panel for error reporting
      SwitchPanel(validatePanel);

      // Error - shift to offending input widget
      inputWidget.focus();
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

function SetAlign(selectWidgetID, defaultIndex, element, attName)
{
  selectWidget = document.getElementById(selectWidgetID);
  value = selectWidget.value;
dump("SetAlign value = "+value+"\n");
  var index = selectWidget.selectedIndex;
  if (index == defaultIndex)
    element.removeAttribute(attName);
  else
    element.setAttribute(attName, value);
}

function ValidateTableData()
{
  validatePanel = TablePanel;
  var newRowCount = ValidateNumber("TableRowsInput", null, 1, rowCount, null, null);
  if (error) return false;

  var newColCount = ValidateNumber("TableColumnsInput", null, 1, colCount, null, null);
  if (error) return false;

  // TODO: ADD/DELETE ROWS/COLUMNS
  var temp = ValidateNumber("TableHeightInput", dialog.TableHeightUnits, 
                            1, maxPixels, globalTableElement, "height");
  if (error) return false;
  temp = ValidateNumber("TableWidthInput", dialog.TableWidthUnits, 
                         1, maxPixels, globalTableElement, "width");
  if (error) return false;

  var border = ValidateNumber("BorderWidthInput", null, 0, maxPixels, globalTableElement, "border");
  // TODO: Deal with "BORDER" without value issue
  if (error) return false;

  temp = ValidateNumber("SpacingInput", null, 0, maxPixels, globalTableElement, "cellspacing");
  if (error) return false;

  temp = ValidateNumber("PaddingInput", null, 0, maxPixels, globalTableElement, "cellpadding");
  if (error) return false;

  SetAlign("TableAlignSelect", defHAlignIndex, globalTableElement, "align");

  // Color is set on globalCellElement immediately

  // Background Image
  var imageName = dialog.TableImageInput.value.trimString();
  if (imageName && imageName != "")
  {
    if (IsValidImage(imageName))
      globalTableElement.background = imageName;
    else
    {
      dialog.TableImageInput.focus();
      // Switch to appropriate panel for error reporting
      SwitchPanel(validatePanel);
      ShowInputErrorMessage(GetString("MissingImageError"));
      return false;
    }
  } else {
    globalTableElement.removeAttribute("background");
  }

  return true;
}

function ValidateCellData()
{

  validatePanel = CellPanel;
  var temp;
  temp = ValidateNumber("CellHeightInput", dialog.TableHeightUnits, 
                         1, maxPixels, globalCellElement, "height");
  if (error) return false;
  temp = ValidateNumber("CellWidthInput", dialog.TableWidthUnits, 
                         1, maxPixels, globalCellElement, "width");
  if (error) return false;
  
  // Vertical alignment is complicated by "char" type
  var hAlign = dialog.CellHAlignSelect.value;
dump("Cell hAlign = "+hAlign+"\n");

  var index = dialog.CellHAlignSelect.selectedIndex;
dump("HAlign index="+index+"\n");
  if (index == defHAlignIndex)
  {
    globalCellElement.removeAttribute("hAlign");
    globalCellElement.removeAttribute(charStr);
  }
  else
  {
    if (index == charIndex)
    {
      var alignChar = dialog.CellAlignCharInput.value.trimString();
       globalCellElement.setAttribute(charStr, alignChar);
dump("Alignment char="+alignChar+" Align Value="+dialog.CellHAlignSelect.value+"\n");
    }

    globalCellElement.setAttribute("align",dialog.CellHAlignSelect.value);
  }

  SetAlign("CellVAlignSelect", defVAlignIndex, globalCellElement, "valign");

  var shouldBeHeader = dialog.HeaderCheck.checked;
  if (shouldBeHeader != CellIsHeader)
  {
    //TODO: THIS IS MESSY! Convert exisisting TD to TH or vice versa
  }
  
  if (dialog.NoWrapCheck.checked)
    globalCellElement.setAttribute("nowrap","true");
  else
    globalCellElement.removeAttribute("nowrap");

  // Background Image
  var imageName = dialog.TableImageInput.value.trimString();
  if (imageName && imageName != "")
  {
    if (IsValidImage(imageName))
      globalCellElement.background = imageName;
    else
    {
      dialog.CellImageInput.focus();
      // Switch to appropriate panel for error reporting
      SwitchPanel(validatePanel);
      ShowInputErrorMessage(GetString("MissingImageError"));
    }
  } else {
    globalCellElement.removeAttribute("background");
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

function onOK()
{
  if (ValidateData())
  {
    editorShell.BeginBatchChanges();
    editorShell.CloneAttributes(TableElement, globalTableElement);
    // Apply changes to all selected cells
    var selectedCell = editorShell.GetFirstSelectedCell();
    while (selectedCell)
    {
      // TODO: We need to change this to set only particular attributes
      // Set an "ignore" attribute on the attribute node?
      editorShell.CloneAttributes(selectedCell,globalCellElement); 
      selectedCell = editorShell.GetNextSelectedCell();
    }
    //TODO: DOM manipulation to add/remove table rows/columns,
    //      Switch cell type to TH -- involves removing TD and replacing with TD
    //      Creating and moving CAPTION element
            
    editorShell.EndBatchChanges();
    return true;
  }
  return false;
}
