/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger
 *   Charles Manske (cmanske@netscape.com)
 *   Neil Rashbrook (neil@parkwaycc.co.uk)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//Cancel() is in EdDialogCommon.js
var gTableElement;
var gCellElement;
var gTableCaptionElement;
var globalCellElement;
var globalTableElement
var gValidateTab;
const defHAlign =   "left";
const centerStr =   "center";  //Index=1
const rightStr =    "right";   // 2
const justifyStr =  "justify"; // 3
const charStr =     "char";    // 4
const defVAlign =   "middle";
const topStr =      "top";
const bottomStr =   "bottom";
const bgcolor = "bgcolor";
var gTableColor;
var gCellColor;

const cssBackgroundColorStr = "background-color";

var gRowCount = 1;
var gColCount = 1;
var gLastRowIndex;
var gLastColIndex;
var gNewRowCount;
var gNewColCount;
var gCurRowIndex;
var gCurColIndex;
var gCurColSpan;
var gSelectedCellsType = 1;
const SELECT_CELL = 1;
const SELECT_ROW = 2;
const SELECT_COLUMN = 3;
const RESET_SELECTION = 0;
var gCellData = { value:null, startRowIndex:0, startColIndex:0, rowSpan:0, colSpan:0,
                 actualRowSpan:0, actualColSpan:0, isSelected:false
               };
var gAdvancedEditUsed;
var gAlignWasChar = false;

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
var gApplyUsed = false;
var gSelection;
var gCellDataChanged = false;
var gCanDelete = false;
var gPrefs = GetPrefs();
var gUseCSS = true;
var gActiveEditor;

// dialog initialization code
function Startup()
{
  gActiveEditor = GetCurrentTableEditor();
  if (!gActiveEditor)
  {
    window.close();
    return;
  }

  try {
    gSelection = gActiveEditor.selection;
  } catch (e) {}
  if (!gSelection) return;

  // Get dialog widgets - Table Panel
  gDialog.TableRowsInput = document.getElementById("TableRowsInput");
  gDialog.TableColumnsInput = document.getElementById("TableColumnsInput");
  gDialog.TableWidthInput = document.getElementById("TableWidthInput");
  gDialog.TableWidthUnits = document.getElementById("TableWidthUnits");
  gDialog.TableHeightInput = document.getElementById("TableHeightInput");
  gDialog.TableHeightUnits = document.getElementById("TableHeightUnits");
  try {
    if (!gPrefs.getBoolPref("editor.use_css") || (gActiveEditor.flags & 1))
    {
      gUseCSS = false;
      var tableHeightLabel = document.getElementById("TableHeightLabel");
      tableHeightLabel.parentNode.removeChild(tableHeightLabel);
      gDialog.TableHeightInput.parentNode.removeChild(gDialog.TableHeightInput);
      gDialog.TableHeightUnits.parentNode.removeChild(gDialog.TableHeightUnits);
    }
  } catch (e) {}
  gDialog.BorderWidthInput = document.getElementById("BorderWidthInput");
  gDialog.SpacingInput = document.getElementById("SpacingInput");
  gDialog.PaddingInput = document.getElementById("PaddingInput");
  gDialog.TableAlignList = document.getElementById("TableAlignList");
  gDialog.TableCaptionList = document.getElementById("TableCaptionList");
  gDialog.TableInheritColor = document.getElementById("TableInheritColor");
  gDialog.TabBox =  document.getElementById("TabBox");

  // Cell Panel
  gDialog.SelectionList = document.getElementById("SelectionList");
  gDialog.PreviousButton = document.getElementById("PreviousButton");
  gDialog.NextButton = document.getElementById("NextButton");
  // Currently, we always apply changes and load new attributes when changing selection
  // (Let's keep this for possible future use)
  //gDialog.ApplyBeforeMove =  document.getElementById("ApplyBeforeMove");
  //gDialog.KeepCurrentData = document.getElementById("KeepCurrentData");

  gDialog.CellHeightInput = document.getElementById("CellHeightInput");
  gDialog.CellHeightUnits = document.getElementById("CellHeightUnits");
  gDialog.CellWidthInput = document.getElementById("CellWidthInput");
  gDialog.CellWidthUnits = document.getElementById("CellWidthUnits");
  gDialog.CellHAlignList = document.getElementById("CellHAlignList");
  gDialog.CellVAlignList = document.getElementById("CellVAlignList");
  gDialog.CellInheritColor = document.getElementById("CellInheritColor");
  gDialog.CellStyleList = document.getElementById("CellStyleList");
  gDialog.TextWrapList = document.getElementById("TextWrapList");

  // In cell panel, user must tell us which attributes to apply via checkboxes,
  //  else we would apply values from one cell to ALL in selection
  //  and that's probably not what they expect!
  gDialog.CellHeightCheckbox = document.getElementById("CellHeightCheckbox");
  gDialog.CellWidthCheckbox = document.getElementById("CellWidthCheckbox");
  gDialog.CellHAlignCheckbox = document.getElementById("CellHAlignCheckbox");
  gDialog.CellVAlignCheckbox = document.getElementById("CellVAlignCheckbox");
  gDialog.CellStyleCheckbox = document.getElementById("CellStyleCheckbox");
  gDialog.TextWrapCheckbox = document.getElementById("TextWrapCheckbox");
  gDialog.CellColorCheckbox = document.getElementById("CellColorCheckbox");
  gDialog.TableTab = document.getElementById("TableTab");
  gDialog.CellTab = document.getElementById("CellTab");
  gDialog.AdvancedEditCell = document.getElementById("AdvancedEditButton2");
  // Save "normal" tooltip message for Advanced Edit button
  gDialog.AdvancedEditCellToolTipText = gDialog.AdvancedEditCell.getAttribute("tooltiptext");

  try {
    gTableElement = gActiveEditor.getElementOrParentByTagName("table", null);
  } catch (e) {}
  if(!gTableElement)
  {
    dump("Failed to get table element!\n");
    window.close();
    return;
  }
  globalTableElement = gTableElement.cloneNode(false);

  var tagNameObj = { value: "" };
  var countObj = { value : 0 };
  var tableOrCellElement;
  try {
   tableOrCellElement = gActiveEditor.getSelectedOrParentTableElement(tagNameObj, countObj);
  } catch (e) {}

  if (tagNameObj.value == "td")
  {
    // We are in a cell
    gSelectedCellCount = countObj.value;
    gCellElement = tableOrCellElement;
    globalCellElement = gCellElement.cloneNode(false);

    // Tells us whether cell, row, or column is selected
    try {
      gSelectedCellsType = gActiveEditor.getSelectedCellsType(gTableElement);
    } catch (e) {}

    // Ignore types except Cell, Row, and Column
    if (gSelectedCellsType < SELECT_CELL || gSelectedCellsType > SELECT_COLUMN)
      gSelectedCellsType = SELECT_CELL;

    // Be sure at least 1 cell is selected.
    // (If the count is 0, then we were inside the cell.)
    if (gSelectedCellCount == 0)
      DoCellSelection();

    // Get location in the cell map
    var rowIndexObj = { value: 0 };
    var colIndexObj = { value: 0 };
    try {
      gActiveEditor.getCellIndexes(gCellElement, rowIndexObj, colIndexObj);
    } catch (e) {}
    gCurRowIndex = rowIndexObj.value;
    gCurColIndex = colIndexObj.value;

    // We save the current colspan to quickly
    //  move selection from from cell to cell
    if (GetCellData(gCurRowIndex, gCurColIndex))
      gCurColSpan = gCellData.colSpan;

    // Starting TabPanel name is passed in
    if (window.arguments[1] == "CellPanel")
      gDialog.TabBox.selectedTab = gDialog.CellTab;
  }

  if (gDialog.TabBox.selectedTab == gDialog.TableTab)
  {
    // We may call this with table selected, but no cell,
    //  so disable the Cell Properties tab
    if(!gCellElement)
    {
      // XXX: Disabling of tabs is currently broken, so for
      //      now we'll just remove the tab completely.
      //gDialog.CellTab.disabled = true;
      gDialog.CellTab.parentNode.removeChild(gDialog.CellTab);
    }
  }

  // Note: we must use gTableElement, not globalTableElement for these,
  //  thus we should not put this in InitDialog.
  // Instead, monitor desired counts with separate globals
  var rowCountObj = { value: 0 };
  var colCountObj = { value: 0 };
  try {
    gActiveEditor.getTableSize(gTableElement, rowCountObj, colCountObj);
  } catch (e) {}

  gRowCount = rowCountObj.value;
  gLastRowIndex = gRowCount-1;
  gColCount = colCountObj.value;
  gLastColIndex = gColCount-1;


  // Set appropriate icons and enable state for the Previous/Next buttons
  SetSelectionButtons();

  // If only one cell in table, disable change-selection widgets
  if (gRowCount == 1 && gColCount == 1)
    gDialog.SelectionList.setAttribute("disabled", "true");

  // User can change these via textboxes
  gNewRowCount = gRowCount;
  gNewColCount = gColCount;

  // This flag is used to control whether set check state
  //  on "set attribute" checkboxes
  // (Advanced Edit dialog use calls  InitDialog when done)
  gAdvancedEditUsed = false;
  InitDialog();
  gAdvancedEditUsed = true;

  // If first initializing, we really aren't changing anything
  gCellDataChanged = false;

  if (gDialog.TabBox.selectedTab == gDialog.CellTab)
    setTimeout("gDialog.SelectionList.focus()", 0);
  else
    SetTextboxFocus(gDialog.TableRowsInput);

  SetWindowLocation();
}


function InitDialog()
{
// turn on extra1 to be "apply"
  var applyButton = document.documentElement.getButton("extra1");
  if (applyButton)
  {
    applyButton.label = GetString("Apply");
    applyButton.setAttribute("accesskey", GetString("ApplyAccessKey"));
  }
  
  // Get Table attributes
  gDialog.TableRowsInput.value = gRowCount;
  gDialog.TableColumnsInput.value = gColCount;
  gDialog.TableWidthInput.value = InitPixelOrPercentMenulist(globalTableElement, gTableElement, "width", "TableWidthUnits", gPercent);
  if (gUseCSS) {
    gDialog.TableHeightInput.value = InitPixelOrPercentMenulist(globalTableElement, gTableElement, "height",
                                                                "TableHeightUnits", gPercent);
  }
  gDialog.BorderWidthInput.value = globalTableElement.border;
  gDialog.SpacingInput.value = globalTableElement.cellSpacing;
  gDialog.PaddingInput.value = globalTableElement.cellPadding;

  var marginLeft  = GetHTMLOrCSSStyleValue(globalTableElement, "align", "margin-left");
  var marginRight = GetHTMLOrCSSStyleValue(globalTableElement, "align", "margin-right");
  var halign = marginLeft.toLowerCase() + " " + marginRight.toLowerCase();
  if (halign == "center center" || halign == "auto auto")
    gDialog.TableAlignList.value = "center";
  else if (halign == "right right" || halign == "auto 0px")
    gDialog.TableAlignList.value = "right";
  else // Default = left
    gDialog.TableAlignList.value = "left";

  // Be sure to get caption from table in doc, not the copied "globalTableElement"
  gTableCaptionElement = gTableElement.caption;
  if (gTableCaptionElement)
  {
    var align = GetHTMLOrCSSStyleValue(gTableCaptionElement, "align", "caption-side");
    if (align != "bottom" && align != "left" && align != "right")
      align = "top";
    gDialog.TableCaptionList.value = align;
  }

  gTableColor = GetHTMLOrCSSStyleValue(globalTableElement, bgcolor, cssBackgroundColorStr);
  gTableColor = ConvertRGBColorIntoHEXColor(gTableColor);
  SetColor("tableBackgroundCW", gTableColor);

  InitCellPanel();
}

function InitCellPanel()
{
  // Get cell attributes
  if (globalCellElement)
  {
    // This assumes order of items is Cell, Row, Column
    gDialog.SelectionList.value = gSelectedCellsType;

    var previousValue = gDialog.CellHeightInput.value;
    gDialog.CellHeightInput.value = InitPixelOrPercentMenulist(globalCellElement, gCellElement, "height", "CellHeightUnits", gPixel);
    gDialog.CellHeightCheckbox.checked = gAdvancedEditUsed && previousValue != gDialog.CellHeightInput.value;

    previousValue= gDialog.CellWidthInput.value;
    gDialog.CellWidthInput.value = InitPixelOrPercentMenulist(globalCellElement, gCellElement, "width", "CellWidthUnits", gPixel);
    gDialog.CellWidthCheckbox.checked = gAdvancedEditUsed && previousValue != gDialog.CellWidthInput.value;

    var previousIndex = gDialog.CellVAlignList.selectedIndex;
    var valign = GetHTMLOrCSSStyleValue(globalCellElement, "valign", "vertical-align").toLowerCase();
    if (valign == topStr || valign == bottomStr)
      gDialog.CellVAlignList.value = valign;
    else // Default = middle
      gDialog.CellVAlignList.value = defVAlign;

    gDialog.CellVAlignCheckbox.checked = gAdvancedEditUsed && previousIndex != gDialog.CellVAlignList.selectedIndex;

    previousIndex = gDialog.CellHAlignList.selectedIndex;

    gAlignWasChar = false;

    var halign = GetHTMLOrCSSStyleValue(globalCellElement, "align", "text-align").toLowerCase();
    switch (halign)
    {
      case centerStr:
      case rightStr:
      case justifyStr:
        gDialog.CellHAlignList.value = halign;
        break;
      case charStr:
        // We don't support UI for this because layout doesn't work: bug 2212.
        // Remember that's what they had so we don't change it
        //  unless they change the alignment by using the menulist
        gAlignWasChar = true;
        // Fall through to use show default alignment in menu
      default:
        // Default depends on cell type (TH is "center", TD is "left")
        gDialog.CellHAlignList.value =
          (globalCellElement.nodeName.toLowerCase() == "th") ? "center" : "left";
        break;
    }

    gDialog.CellHAlignCheckbox.checked = gAdvancedEditUsed &&
      previousIndex != gDialog.CellHAlignList.selectedIndex;

    previousIndex = gDialog.CellStyleList.selectedIndex;
    gDialog.CellStyleList.value = globalCellElement.nodeName.toLowerCase();
    gDialog.CellStyleCheckbox.checked = gAdvancedEditUsed && previousIndex != gDialog.CellStyleList.selectedIndex;

    previousIndex = gDialog.TextWrapList.selectedIndex;
    if (GetHTMLOrCSSStyleValue(globalCellElement, "nowrap", "white-space") == "nowrap")
      gDialog.TextWrapList.value = "nowrap";
    else
      gDialog.TextWrapList.value = "wrap";
    gDialog.TextWrapCheckbox.checked = gAdvancedEditUsed && previousIndex != gDialog.TextWrapList.selectedIndex;

    previousValue = gCellColor;
    gCellColor = GetHTMLOrCSSStyleValue(globalCellElement, bgcolor, cssBackgroundColorStr);
    gCellColor = ConvertRGBColorIntoHEXColor(gCellColor);
    SetColor("cellBackgroundCW", gCellColor);
    gDialog.CellColorCheckbox.checked = gAdvancedEditUsed && previousValue != gCellColor;

    // We want to set this true in case changes came
    //   from Advanced Edit dialog session (must assume something changed)
    gCellDataChanged = true;
  }
}

function GetCellData(rowIndex, colIndex)
{
  // Get actual rowspan and colspan
  var startRowIndexObj = { value: 0 };
  var startColIndexObj = { value: 0 };
  var rowSpanObj = { value: 0 };
  var colSpanObj = { value: 0 };
  var actualRowSpanObj = { value: 0 };
  var actualColSpanObj = { value: 0 };
  var isSelectedObj = { value: false };

  try {
    gActiveEditor.getCellDataAt(gTableElement, rowIndex, colIndex,
                         gCellData,
                         startRowIndexObj, startColIndexObj,
                         rowSpanObj, colSpanObj,
                         actualRowSpanObj, actualColSpanObj, isSelectedObj);
    // We didn't find a cell
    if (!gCellData.value) return false;
  }
  catch(ex) {
    return false;
  }

  gCellData.startRowIndex = startRowIndexObj.value;
  gCellData.startColIndex = startColIndexObj.value;
  gCellData.rowSpan = rowSpanObj.value;
  gCellData.colSpan = colSpanObj.value;
  gCellData.actualRowSpan = actualRowSpanObj.value;
  gCellData.actualColSpan = actualColSpanObj.value;
  gCellData.isSelected = isSelectedObj.value;
  return true;
}

function SelectCellHAlign()
{
  SetCheckbox("CellHAlignCheckbox");
  // Once user changes the alignment,
  //  we loose their original "CharAt" alignment"
  gAlignWasChar = false;
}

function GetColorAndUpdate(ColorWellID)
{
  var colorWell = document.getElementById(ColorWellID);
  if (!colorWell) return;

  var colorObj = { Type:"", TableColor:0, CellColor:0, NoDefault:false, Cancel:false, BackgroundColor:0 };

  switch( ColorWellID )
  {
    case "tableBackgroundCW":
      colorObj.Type = "Table";
      colorObj.TableColor = gTableColor;
      break;
    case "cellBackgroundCW":
      colorObj.Type = "Cell";
      colorObj.CellColor = gCellColor;
      break;
  }
  window.openDialog("chrome://editor/content/EdColorPicker.xul", "_blank", "chrome,close,titlebar,modal", "", colorObj);

  // User canceled the dialog
  if (colorObj.Cancel)
    return;

  switch( ColorWellID )
  {
    case "tableBackgroundCW":
      gTableColor = colorObj.BackgroundColor;
      SetColor(ColorWellID, gTableColor);
      break;
    case "cellBackgroundCW":
      gCellColor = colorObj.BackgroundColor;
      SetColor(ColorWellID, gCellColor);
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
      try {
        gActiveEditor.setAttributeOrEquivalent(globalCellElement, bgcolor,
                                               color, true);
      } catch(e) {}
      gDialog.CellInheritColor.collapsed = true;
    }
    else
    {
      try {
        gActiveEditor.removeAttributeOrEquivalent(globalCellElement, bgcolor, true);
      } catch(e) {}
      // Reveal addition message explaining "default" color
      gDialog.CellInheritColor.collapsed = false;
    }
  }
  else
  {
    if (color)
    {
      try {
        gActiveEditor.setAttributeOrEquivalent(globalTableElement, bgcolor,
                                               color, true);
      } catch(e) {}
      gDialog.TableInheritColor.collapsed = true;
    }
    else
    {
      try {
        gActiveEditor.removeAttributeOrEquivalent(globalTableElement, bgcolor, true);
      } catch(e) {}
      gDialog.TableInheritColor.collapsed = false;
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
  gCellElement = gCellData.value;
  globalCellElement = gCellElement;

  gCurRowIndex = 0;
  gCurColIndex = 0;
  ChangeSelection(RESET_SELECTION);
}

function ChangeSelection(newType)
{
  newType = Number(newType);

  if (gSelectedCellsType == newType)
    return;

  if (newType == RESET_SELECTION)
    // Restore selection to existing focus cell
    gSelection.collapse(gCellElement,0);
  else
    gSelectedCellsType = newType;

  // Keep the same focus gCellElement, just change the type
  DoCellSelection();
  SetSelectionButtons();

  // Note: globalCellElement should still be a clone of gCellElement
}

function MoveSelection(forward)
{
  var newRowIndex = gCurRowIndex;
  var newColIndex = gCurColIndex;
  var focusCell;
  var inRow = false;

  if (gSelectedCellsType == SELECT_ROW)
  {
    newRowIndex += (forward ? 1 : -1);

    // Wrap around if before first or after last row
    if (newRowIndex < 0)
      newRowIndex = gLastRowIndex;
    else if (newRowIndex > gLastRowIndex)
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

    if (gSelectedCellsType == SELECT_CELL)
    {
      // Skip to next cell
      if (forward)
        newColIndex += gCurColSpan;
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
      newColIndex = gLastColIndex;

      if (gSelectedCellsType == SELECT_CELL)
      {
        // If moving by cell, also wrap to previous...
        if (newRowIndex > 0)
          newRowIndex -= 1;
        else
          // ...or the last row
          newRowIndex = gLastRowIndex;

        inRow = true;
      }
    }
    else if (newColIndex > gLastColIndex)
    {
      // Request is after the last cell in column

      // Wrap to first cell in column
      newColIndex = 0;

      if (gSelectedCellsType == SELECT_CELL)
      {
        // If moving by cell, also wrap to next...
        if (newRowIndex < gLastRowIndex)
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
      if (gCellData.startRowIndex == newRowIndex)
        break;
      else
        // Cell spans from a row above, look for the next cell in row
        newRowIndex += gCellData.actualRowSpan;
    }
    else
    {
      if (gCellData.startColIndex == newColIndex)
        break;
      else
        // Cell spans from a Col above, look for the next cell in column
        newColIndex += gCellData.actualColSpan;
    }
  }
  while(true);

  // Save data for current selection before changing
  if (gCellDataChanged) // && gDialog.ApplyBeforeMove.checked)
  {
    if (!ValidateCellData())
      return;

    gActiveEditor.beginTransaction();
    // Apply changes to all selected cells
    ApplyCellAttributes();
    gActiveEditor.endTransaction();

    SetCloseButton();
  }

  // Set cell and other data for new selection
  gCellElement = gCellData.value;

  // Save globals for new current cell
  gCurRowIndex = gCellData.startRowIndex;
  gCurColIndex = gCellData.startColIndex;
  gCurColSpan = gCellData.actualColSpan;

  // Copy for new global cell
  globalCellElement = gCellElement.cloneNode(false);

  // Change the selection
  DoCellSelection();

  // Scroll page so new selection is visible
  // Using SELECTION_ANCHOR_REGION makes the upper-left corner of first selected cell
  //    the point to bring into view.
  try {
    var selectionController = gActiveEditor.selectionController;
    selectionController.scrollSelectionIntoView(selectionController.SELECTION_NORMAL, selectionController.SELECTION_ANCHOR_REGION, true);
  } catch (e) {}

  // Reinitialize dialog using new cell
//  if (!gDialog.KeepCurrentData.checked)
  // Setting this false unchecks all "set attributes" checkboxes
  gAdvancedEditUsed = false;
  InitCellPanel();
  gAdvancedEditUsed = true;
}


function DoCellSelection()
{
  // Collapse selection into to the focus cell
  //  so editor uses that as start cell
  gSelection.collapse(gCellElement, 0);

  var tagNameObj = { value: "" };
  var countObj = { value: 0 };
  try {
    switch (gSelectedCellsType)
    {
      case SELECT_CELL:
        gActiveEditor.selectTableCell();
        break
      case SELECT_ROW:
        gActiveEditor.selectTableRow();
        break;
      default:
        gActiveEditor.selectTableColumn();
        break;
    }
    // Get number of cells selected
    var tableOrCellElement = gActiveEditor.getSelectedOrParentTableElement(tagNameObj, countObj);
  } catch (e) {}

  if (tagNameObj.value == "td")
    gSelectedCellCount = countObj.value;
  else
    gSelectedCellCount = 0;

  // Currently, we can only allow advanced editing on ONE cell element at a time
  //   else we ignore CSS, JS, and HTML attributes not already in dialog
  SetElementEnabled(gDialog.AdvancedEditCell, gSelectedCellCount == 1);

  gDialog.AdvancedEditCell.setAttribute("tooltiptext", 
    gSelectedCellCount > 1 ? GetString("AdvancedEditForCellMsg") :
                             gDialog.AdvancedEditCellToolTipText);
}

function SetSelectionButtons()
{
  if (gSelectedCellsType == SELECT_ROW)
  {
    // Trigger CSS to set images of up and down arrows
    gDialog.PreviousButton.setAttribute("type","row");
    gDialog.NextButton.setAttribute("type","row");
  }
  else
  {
    // or images of left and right arrows
    gDialog.PreviousButton.setAttribute("type","col");
    gDialog.NextButton.setAttribute("type","col");
  }
  DisableSelectionButtons((gSelectedCellsType == SELECT_ROW && gRowCount == 1) ||
                          (gSelectedCellsType == SELECT_COLUMN && gColCount == 1) ||
                          (gRowCount == 1 && gColCount == 1));
}

function DisableSelectionButtons( disable )
{
  gDialog.PreviousButton.setAttribute("disabled", disable ? "true" : "false");
  gDialog.NextButton.setAttribute("disabled", disable ? "true" : "false");
}

function SwitchToValidatePanel()
{
  if (gDialog.TabBox.selectedTab != gValidateTab)
    gDialog.TabBox.selectedTab = gValidateTab;
}

function SetAlign(listID, defaultValue, element, attName)
{
  var value = document.getElementById(listID).value;
  if (value == defaultValue)
  {
    try {
      gActiveEditor.removeAttributeOrEquivalent(element, attName, true);
    } catch(e) {}
  }
  else
  {
    try {
      gActiveEditor.setAttributeOrEquivalent(element, attName, value, true);
    } catch(e) {}
  }
}

function ValidateTableData()
{
  gValidateTab = gDialog.TableTab;
  gNewRowCount = Number(ValidateNumber(gDialog.TableRowsInput, null, 1, gMaxRows, null, true, true));
  if (gValidationError) return false;

  gNewColCount = Number(ValidateNumber(gDialog.TableColumnsInput, null, 1, gMaxColumns, null, true, true));
  if (gValidationError) return false;

  // If user is deleting any cells, get confirmation
  // (This is a global to the dialog and we ask only once per dialog session)
  if ( !gCanDelete &&
        (gNewRowCount < gRowCount ||
         gNewColCount < gColCount) ) 
  {
    if (ConfirmWithTitle(GetString("DeleteTableTitle"), 
                         GetString("DeleteTableMsg"),
                         GetString("DeleteCells")) )
    {
      gCanDelete = true;
    }
    else
    {
      SetTextboxFocus(gNewRowCount < gRowCount ? gDialog.TableRowsInput : gDialog.TableColumnsInput);
      return false;
    }
  }

  ValidateNumber(gDialog.TableWidthInput, gDialog.TableWidthUnits,
                 1, gMaxTableSize, globalTableElement, "width");
  if (gValidationError) return false;

  if (gUseCSS) {
    ValidateNumber(gDialog.TableHeightInput, gDialog.TableHeightUnits,
                   1, gMaxTableSize, globalTableElement, "height");
    if (gValidationError) return false;
  }

  var border = ValidateNumber(gDialog.BorderWidthInput, null, 0, gMaxPixels, globalTableElement, "border");
  // TODO: Deal with "BORDER" without value issue
  if (gValidationError) return false;

  ValidateNumber(gDialog.SpacingInput, null, 0, gMaxPixels, globalTableElement, "cellspacing");
  if (gValidationError) return false;

  ValidateNumber(gDialog.PaddingInput, null, 0, gMaxPixels, globalTableElement, "cellpadding");
  if (gValidationError) return false;

  SetAlign("TableAlignList", defHAlign, globalTableElement, "align");

  // Color is set on globalCellElement immediately
  return true;
}

function ValidateCellData()
{

  gValidateTab = gDialog.CellTab;

  if (gDialog.CellHeightCheckbox.checked)
  {
    ValidateNumber(gDialog.CellHeightInput, gDialog.CellHeightUnits,
                    1, gMaxTableSize, globalCellElement, "height");
    if (gValidationError) return false;
  }

  if (gDialog.CellWidthCheckbox.checked)
  {
    ValidateNumber(gDialog.CellWidthInput, gDialog.CellWidthUnits,
                   1, gMaxTableSize, globalCellElement, "width");
    if (gValidationError) return false;
  }

  if (gDialog.CellHAlignCheckbox.checked)
  {
    var hAlign = gDialog.CellHAlignList.value;

    // Horizontal alignment is complicated by "char" type
    // We don't change current values if user didn't edit alignment
    if (!gAlignWasChar)
    {
      globalCellElement.removeAttribute(charStr);

      // Always set "align" attribute,
      //  so the default "left" is effective in a cell
      //  when parent row has align set.
      globalCellElement.setAttribute("align", hAlign);
    }
  }

  if (gDialog.CellVAlignCheckbox.checked)
  {
    // Always set valign (no default in 2nd param) so
    //  the default "middle" is effective in a cell
    //  when parent row has valign set.
    SetAlign("CellVAlignList", "", globalCellElement, "valign");
  }

  if (gDialog.TextWrapCheckbox.checked)
  {
    if (gDialog.TextWrapList.value == "nowrap")
      try {
        gActiveEditor.setAttributeOrEquivalent(globalCellElement, "nowrap",
                                               "nowrap", true);
      } catch(e) {}
    else
      try {
        gActiveEditor.removeAttributeOrEquivalent(globalCellElement, "nowrap", true);
      } catch(e) {}
  }

  return true;
}

function ValidateData()
{
  var result;

  // Validate current panel first
  if (gDialog.TabBox.selectedTab == gDialog.TableTab)
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

  // Set global element for AdvancedEdit
  if(gDialog.TabBox.selectedTab == gDialog.TableTab)
    globalElement = globalTableElement;
  else
    globalElement = globalCellElement;

  return true;
}

function ChangeCellTextbox(textboxID)
{
  // Filter input for just integers
  forceInteger(textboxID);

  if (gDialog.TabBox.selectedTab == gDialog.CellTab)
    gCellDataChanged = true;
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
  gCellDataChanged = true;
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
  // Use editor methods since we are always
  //  modifying a table in the document and
  //  we need transaction system for undo
  try {
    if (!value || value.length == 0)
      gActiveEditor.removeAttributeOrEquivalent(destElement, attr, false);
    else
      gActiveEditor.setAttributeOrEquivalent(destElement, attr, value, false);
  } catch(e) {}
}

function ApplyTableAttributes()
{
  var newAlign = gDialog.TableCaptionList.value;
  if (!newAlign) newAlign = "";

  if (gTableCaptionElement)
  {
    // Get current alignment
    var align = GetHTMLOrCSSStyleValue(gTableCaptionElement, "align", "caption-side").toLowerCase();
    // This is the default
    if (!align) align = "top";

    if (newAlign == "")
    {
      // Remove existing caption
      try {
        gActiveEditor.deleteNode(gTableCaptionElement);
      } catch(e) {}
      gTableCaptionElement = null;
    }
    else if(newAlign != align)
    {
      try {
        if (newAlign == "top") // This is default, so don't explicitly set it
          gActiveEditor.removeAttributeOrEquivalent(gTableCaptionElement, "align", false);
        else
          gActiveEditor.setAttributeOrEquivalent(gTableCaptionElement, "align", newAlign, false);
      } catch(e) {}
    }
  }
  else if (newAlign != "")
  {
    // Create and insert a caption:
    try {
      gTableCaptionElement = gActiveEditor.createElementWithDefaults("caption");
    } catch (e) {}
    if (gTableCaptionElement)
    {
      if (newAlign != "top")
        gTableCaptionElement.setAttribute("align", newAlign);

      // Insert it into the table - caption is always inserted as first child
      try {
        gActiveEditor.insertNode(gTableCaptionElement, gTableElement, 0);
      } catch(e) {}

      // Put selecton back where it was
      ChangeSelection(RESET_SELECTION);
    }
  }

  var countDelta;
  var foundCell;
  var i;

  if (gNewRowCount != gRowCount)
  {
    countDelta = gNewRowCount - gRowCount;
    if (gNewRowCount > gRowCount)
    {
      // Append new rows
      // Find first cell in last row
      if(GetCellData(gLastRowIndex, 0))
      {
        try {
          // Move selection to the last cell
          gSelection.collapse(gCellData.value,0);
          // Insert new rows after it
          gActiveEditor.insertTableRow(countDelta, true);
          gRowCount = gNewRowCount;
          gLastRowIndex = gRowCount - 1;
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
      if (gCanDelete)
      {
        // Find first cell starting in first row we delete
        var firstDeleteRow = gRowCount + countDelta;
        foundCell = false;
        for ( i = 0; i <= gLastColIndex; i++)
        {
          if (!GetCellData(firstDeleteRow, i))
            break; // We failed to find a cell

          if (gCellData.startRowIndex == firstDeleteRow)
          {
            foundCell = true;
            break;
          }
        };
        if (foundCell)
        {
          try {
            // Move selection to the cell we found
            gSelection.collapse(gCellData.value, 0);
            gActiveEditor.deleteTableRow(-countDelta);
            gRowCount = gNewRowCount;
            gLastRowIndex = gRowCount - 1;
            if (gCurRowIndex > gLastRowIndex)
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

  if (gNewColCount != gColCount)
  {
    countDelta = gNewColCount - gColCount;

    if (gNewColCount > gColCount)
    {
      // Append new columns
      // Find last cell in first column
      if(GetCellData(0, gLastColIndex))
      {
        try {
          // Move selection to the last cell
          gSelection.collapse(gCellData.value,0);
          gActiveEditor.insertTableColumn(countDelta, true);
          gColCount = gNewColCount;
          gLastColIndex = gColCount-1;
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
      if (gCanDelete)
      {
        var firstDeleteCol = gColCount + countDelta;
        foundCell = false;
        for ( i = 0; i <= gLastRowIndex; i++)
        {
          // Find first cell starting in first column we delete
          if (!GetCellData(i, firstDeleteCol))
            break; // We failed to find a cell

          if (gCellData.startColIndex == firstDeleteCol)
          {
            foundCell = true;
            break;
          }
        };
        if (foundCell)
        {
          try {
            // Move selection to the cell we found
            gSelection.collapse(gCellData.value, 0);
            gActiveEditor.deleteTableColumn(-countDelta);
            gColCount = gNewColCount;
            gLastColIndex = gColCount-1;
            if (gCurColIndex > gLastColIndex)
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
  try {
    gActiveEditor.cloneAttributes(gTableElement, globalTableElement);
  } catch(e) {}
}

function ApplyCellAttributes()
{
  var rangeObj = { value: null };
  var selectedCell;
  try {
    selectedCell = gActiveEditor.getFirstSelectedCell(rangeObj);
  } catch(e) {}

  if (!selectedCell)
    return;

  if (gSelectedCellCount == 1)
  {
    // When only one cell is selected, simply clone entire element,
    //  thus CSS and JS from Advanced edit is copied
    try {
      gActiveEditor.cloneAttributes(selectedCell, globalCellElement);
    } catch(e) {}

    if (gDialog.CellStyleCheckbox.checked)
    {
      var currentStyleIndex = (selectedCell.nodeName.toLowerCase() == "th") ? 1 : 0;
      if (gDialog.CellStyleList.selectedIndex != currentStyleIndex)
      {
        // Switch cell types
        // (replaces with new cell and copies attributes and contents)
        try {
          selectedCell = gActiveEditor.switchTableCellHeaderType(selectedCell);
        } catch(e) {}
      }
    }
  }
  else
  {
    // Apply changes to all selected cells
    //XXX THIS DOESN'T COPY ADVANCED EDIT CHANGES!
    try {
      while (selectedCell)
      {
        ApplyAttributesToOneCell(selectedCell);
        selectedCell = gActiveEditor.getNextSelectedCell(rangeObj);
      }
    } catch(e) {}
  }
  gCellDataChanged = false;
}

function ApplyAttributesToOneCell(destElement)
{
  if (gDialog.CellHeightCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "height");

  if (gDialog.CellWidthCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "width");

  if (gDialog.CellHAlignCheckbox.checked)
  {
    CloneAttribute(destElement, globalCellElement, "align");
    CloneAttribute(destElement, globalCellElement, charStr);
  }

  if (gDialog.CellVAlignCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "valign");

  if (gDialog.TextWrapCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "nowrap");

  if (gDialog.CellStyleCheckbox.checked)
  {
    var newStyleIndex = gDialog.CellStyleList.selectedIndex;
    var currentStyleIndex = (destElement.nodeName.toLowerCase() == "th") ? 1 : 0;

    if (newStyleIndex != currentStyleIndex)
    {
      // Switch cell types
      // (replaces with new cell and copies attributes and contents)
      try {
        destElement = gActiveEditor.switchTableCellHeaderType(destElement);
      } catch(e) {}
    }
  }

  if (gDialog.CellColorCheckbox.checked)
    CloneAttribute(destElement, globalCellElement, "bgcolor");
}

function SetCloseButton()
{
  // Change text on "Cancel" button after Apply is used
  if (!gApplyUsed)
  {
    document.documentElement.getButton("cancel").setAttribute("label",GetString("Close"));
    gApplyUsed = true;
  }
}

function Apply()
{
  if (ValidateData())
  {
    gActiveEditor.beginTransaction();

    ApplyTableAttributes();

    // We may have just a table, so check for cell element
    if (globalCellElement)
      ApplyCellAttributes();

    gActiveEditor.endTransaction();

    SetCloseButton();
    return true;
  }
  return false;
}

function doHelpButton()
{
  openHelp("table_properties");
}

function onAccept()
{
  // Do same as Apply and close window if ValidateData succeeded
  var retVal = Apply();
  if (retVal)
    SaveWindowLocation();

  return retVal;
}
