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
var rowCount = 1;
var colCount = 1;
var newRowCount;
var newColCount;
var selectedCellCount = 0;
var error = 0;
var ApplyUsed = false;
// What should these be?
var maxRows    = 10000;
var maxColumns = 10000;

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
  dialog.TableAlignList = document.getElementById("TableAlignList");
  dialog.TableCaptionList = document.getElementById("TableCaptionList");
  dialog.TableInheritColor = document.getElementById("TableInheritColor");
  dialog.TableImageInput = document.getElementById("TableImageInput");
  dialog.TableImageButton = document.getElementById("TableImageButton");

  dialog.RowsCheckbox = document.getElementById("RowsCheckbox");
  dialog.ColumnsCheckbox = document.getElementById("ColumnsCheckbox");
  dialog.TableHeightCheckbox = document.getElementById("TableHeightCheckbox");
  dialog.TableWidthCheckbox = document.getElementById("TableWidthCheckbox");
  dialog.TableBorderCheckbox = document.getElementById("TableBorderCheckbox");
  dialog.CellSpacingCheckbox = document.getElementById("CellSpacingCheckbox");
  dialog.CellPaddingCheckbox = document.getElementById("CellPaddingCheckbox");
  dialog.TableHAlignCheckbox = document.getElementById("TableHAlignCheckbox");
  dialog.TableCaptionCheckbox = document.getElementById("TableCaptionCheckbox");
  dialog.TableColorCheckbox = document.getElementById("TableColorCheckbox");
  dialog.TableImageCheckbox = document.getElementById("TableImageCheckbox");

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
  dialog.CellHAlignList = document.getElementById("CellHAlignList");
  dialog.CellAlignCharInput = document.getElementById("CellAlignCharInput");
  dialog.CellVAlignList = document.getElementById("CellVAlignList");
  dialog.CellStyleList = document.getElementById("CellStyleList");
  dialog.TextWrapList = document.getElementById("TextWrapList");
  dialog.CellInheritColor = document.getElementById("CellInheritColor");
  dialog.CellImageInput = document.getElementById("CellImageInput");
  dialog.CellImageButton = document.getElementById("CellImageButton");

  dialog.CellHeightCheckbox = document.getElementById("CellHeightCheckbox");
  dialog.CellWidthCheckbox = document.getElementById("CellWidthCheckbox");
  dialog.SpanCheckbox = document.getElementById("SpanCheckbox");
  dialog.CellHAlignCheckbox = document.getElementById("CellHAlignCheckbox");
  dialog.CellVAlignCheckbox = document.getElementById("CellVAlignCheckbox");
  dialog.CellStyleCheckbox = document.getElementById("CellStyleCheckbox");
  dialog.TextWrapCheckbox = document.getElementById("TextWrapCheckbox");
  dialog.CellColorCheckbox = document.getElementById("CellColorCheckbox");
  dialog.CellImageCheckbox = document.getElementById("CellImageCheckbox");

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
    dump("Failed to get table element!\n");
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

  // User can change these via textfields  
  newRowCount = rowCount;
  newColCount = colCount;

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
    dialog.TableAlignList.selectedIndex = 1;
  else if (halign == bottomStr)
    dialog.TableAlignList.selectedIndex = 2;
  else // Default = left
    dialog.TableAlignList.selectedIndex = 0;
  
  TableCaptionElement = globalTableElement.caption;
dump("Caption Element = "+TableCaptionElement+"\n");
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
        } else {
          // "char" align set but no alignment char value
          dialog.CellHAlignList.selectedIndex = 0;
        }
        break;
      default:  // left
        dialog.CellHAlignList.selectedIndex = 0;
        break;
    }
    // Show/hide extra message to explain "default" case
    SelectCellHAlign();

    dialog.CellStyleList.selectedIndex = (globalCellElement.nodeName == "TH") ? 1 : 0;
    dialog.TextWrapList.selectedIndex = globalCellElement.noWrap ? 1 : 0;
    
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

function SelectCellHAlign(checkboxID)
{
dump("dialog.CellHAlignList.selectedIndex ="+dialog.CellHAlignList.selectedIndex+"\n");
  if (dialog.CellHAlignList.selectedIndex == 4)
    dialog.CellAlignCharInput.removeAttribute("collapsed");
  else
    dialog.CellAlignCharInput.setAttribute("collapsed","true");
  SetCheckbox(checkboxID);
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
function ValidateNumber(inputWidgetID, listWidget, minVal, maxVal, element, attName)
{
  var inputWidget = document.getElementById(inputWidgetID);
  // Global error return value
  error = false;
  var maxLimit = maxVal;
  var isPercent = false;

  var numString = inputWidget.value.trimString();
  if (numString && numString != "")
  {
    if (listWidget)
      isPercent = (listWidget.selectedIndex == 1);
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

function SetAlign(listID, defaultValue, element, attName)
{
  var value = document.getElementById(listID).selectedItem.data;
dump("SetAlign value = "+value+"\n");
  if (value == defaultValue)
    element.removeAttribute(attName);
  else
    element.setAttribute(attName, value);
}

function ValidateTableData()
{
  validatePanel = TablePanel;
  if (dialog.RowsCheckbox.checked)
  {
    newRowCount = Number(ValidateNumber("TableRowsInput", null, 1, maxRows, null, null));
    if (error) return false;
  }

  if (dialog.ColumnsCheckbox.checked)
  {
    newColCount = Number(ValidateNumber("TableColumnsInput", null, 1, maxColumns, null, null));
    if (error) return false;
  }

  if (dialog.TableHeightCheckbox.checked)
  {
    ValidateNumber("TableHeightInput", dialog.TableHeightUnits, 
                    1, maxPixels, globalTableElement, "height");
    if (error) return false;
  }

  if (dialog.TableWidthCheckbox.checked)
  {
    ValidateNumber("TableWidthInput", dialog.TableWidthUnits, 
                   1, maxPixels, globalTableElement, "width");
    if (error) return false;
  }

  if (dialog.TableBorderCheckbox.checked)
  {
    var border = ValidateNumber("BorderWidthInput", null, 0, maxPixels, globalTableElement, "border");
    // TODO: Deal with "BORDER" without value issue
    if (error) return false;
  }

  if (dialog.CellSpacingCheckbox.checked)
  {
    ValidateNumber("SpacingInput", null, 0, maxPixels, globalTableElement, "cellspacing");
    if (error) return false;
  }

  if (dialog.CellPaddingCheckbox.checked)
  {
    ValidateNumber("PaddingInput", null, 0, maxPixels, globalTableElement, "cellpadding");
    if (error) return false;
  }

  if (dialog.TableHAlignCheckbox.checked)
    SetAlign("TableAlignList", defHAlign, globalTableElement, "align");

  // Color is set on globalCellElement immediately

  // Background Image
  if (dialog.TableImageCheckbox.checked)
  {
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
  }
  return true;
}

function ValidateCellData()
{

  validatePanel = CellPanel;

  if (dialog.CellHeightCheckbox.checked)
  {
    ValidateNumber("CellHeightInput", dialog.TableHeightUnits, 
                    1, maxPixels, globalCellElement, "height");
    if (error) return false;
  }

  if (dialog.CellWidthCheckbox.checked)
  {
    ValidateNumber("CellWidthInput", dialog.TableWidthUnits, 
                   1, maxPixels, globalCellElement, "width");
    if (error) return false;
  }
  
  if (dialog.SpanCheckbox.checked)
  {
    ValidateNumber("ColSpanInput", null,
                   1, colCount, globalCellElement, "colspan");
    if (error) return false;

    ValidateNumber("RowSpanInput", null,
                   1, rowCount, globalCellElement, "rowspan");
    if (error) return false;
  }

  if (dialog.CellHAlignCheckbox.checked)
  {
    // Vertical alignment is complicated by "char" type
    var hAlign = dialog.CellHAlignList.selectedItem.data;
dump("Cell hAlign = "+hAlign+"\n");

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
        var alignChar = dialog.CellAlignCharInput.value.trimString();
        globalCellElement.setAttribute(charStr, alignChar);
dump("Alignment char="+alignChar+"\n");
      }

      globalCellElement.setAttribute("align", hAlign);
    }
  }

  if (dialog.CellVAlignCheckbox.checked)
    SetAlign("CellVAlignList", defVAlign, globalCellElement, "valign");

  if (dialog.TextWrapCheckbox.checked)
  {
    if (dialog.TextWrapList.selectedIndex == 1)
      globalCellElement.setAttribute("nowrap","true");
    else
      globalCellElement.removeAttribute("nowrap");
  }

  // Background Image
  if (dialog.CellImageCheckbox.checked)
  {
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

// Call this when a textfield or menulist is changed
//   so the checkbox is automatically set
function SetCheckbox(checkboxID)
{
  // Set associated checkbox
  document.getElementById(checkboxID).checked = true;
}

function ChangeIntTextfield(textfieldID, checkboxID)
{
  // Filter input for just integers
  forceInteger(textfieldID);

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
    editorShell.setAttribute(destElement, attr, value);
}

function ApplyTableAttributes()
{
  if (dialog.TableCaptionCheckbox.checked)
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
dump("Insert a table caption...\n");
      // Create and insert a caption:
      TableCaptionElement = editorShell.CreateElementWithDefaults("caption");
      if (TableCaptionElement)
      {
        if (newAlign != "top")
          TableCaptionElement.setAttribute("align", newAlign);
        
        // Insert it into the table - caption is always inserted as first child
        editorShell.InsertElement(TableCaptionElement, TableElement, 0);
      }
    }
  }

  //TODO: DOM manipulation to add/remove table rows/columns
  if (dialog.RowsCheckbox.checked && newRowCount != rowCount)
  {
  }

  if (dialog.ColumnsCheckbox.checked && newColCount != colCount)
  {
  }

  if (dialog.TableHeightCheckbox.checked)
    CloneAttribute(TableElement, globalTableElement, "height");

  if (dialog.TableWidthCheckbox.checked)
    CloneAttribute(TableElement, globalTableElement, "width");

  if (dialog.TableBorderCheckbox.checked)
    CloneAttribute(TableElement, globalTableElement, "border");

  if (dialog.CellSpacingCheckbox.checked)
    CloneAttribute(TableElement, globalTableElement, "cellspacing");

  if (dialog.CellPaddingCheckbox.checked)
    CloneAttribute(TableElement, globalTableElement, "cellpadding");

  if (dialog.TableHAlignCheckbox.checked)
    CloneAttribute(TableElement, globalTableElement, "align");

  if (dialog.TableColorCheckbox.checked)
    CloneAttribute(TableElement, globalTableElement, "bgcolor");

  if (dialog.TableImageCheckbox.checked)
    CloneAttribute(TableElement, globalTableElement, "background");
  
}

function ApplyCellAttributes(destElement)
{
  if (dialog.CellHeightCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "height");

  if (dialog.CellWidthCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "width");

  if (dialog.SpanCheckbox.checked)
  {
    CloneAttribute(destElement, globalCellElement, "rowspan");
    CloneAttribute(destElement, globalCellElement, "colspan");
  }

  if (dialog.CellHAlignCheckbox.checked)
  {
    CloneAttribute(destElement, globalCellElement, "align");
    CloneAttribute(destElement, globalCellElement, charStr);
  }

  if (dialog.CellVAlignCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "valign");

  if (dialog.CellStyleCheckbox.checked)
  {
    var newStyleIndex = dialog.CellStyleList.selectedIndex;
    var currentStyleIndex = (destElement.nodeName == "TH") ? 1 : 0;

    if (newStyleIndex != currentStyleIndex)
    {
      //TODO: THIS IS MESSY! Convert exisisting TD to TH or vice versa
      CurrentStyleIndex = newStyleIndex;
    }
  }

  if (dialog.TextWrapCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "nowrap");

  if (dialog.CellColorCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "bgcolor");

  if (dialog.CellImageCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "background");
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
      // Apply changes to all selected cells
      var selectedCell = editorShell.GetFirstSelectedCell();
      while (selectedCell)
      {
        ApplyCellAttributes(selectedCell); 
        selectedCell = editorShell.GetNextSelectedCell();
      }
    }            
    editorShell.EndBatchChanges();

    // Change text on "Cancel" button after Apply is used
    if (!ApplyUsed)
    {
      document.getElementById("cancel").setAttribute("value",GetString("Close"));
      ApplyUsed = true;
    }
    return true;
  }
  return false;
}

function onOK()
{
  // Do same as Apply and close window if ValidateData succeeded
  return Apply();
}
