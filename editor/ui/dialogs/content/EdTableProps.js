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
var TablePanel;
var CellPanel;
var dialog;
var cellGlobalElement;
var tableGlobalElement

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

  TableElement = editorShell.GetElementOrParentByTagName("table", null);
  CellElement = editorShell.GetElementOrParentByTagName("td", null);
  // We allow a missing cell -- see below
  if(!TableElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }

  var panelName;
  TabPanel = document.getElementById("TabPanel");
  TablePanel = document.getElementById("TablePanel");
  CellPanel =  document.getElementById("CellPanel");
  var TableTab = document.getElementById("TableTab");
  var CellTab = document.getElementById("CellTab");
  
  if (!TabPanel || !TablePanel || !CellPanel || !TableTab || !CellTab)
  {
    dump("Not all dialog controls were found!!!\n");
    window.close;
  }

  // Get the starting TabPanel name
  var StartPanelName = window.arguments[1];

  tableGlobalElement = TableElement.cloneNode(false);
  if (CellElement)
    cellGlobalElement = CellElement.cloneNode(false);

  // Activate the Cell Panel if requested
  if (StartPanelName == "CellPanel")
  {
    // We must have a cell element to start in this panel
    if(!CellElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
    }

    //Set index for starting panel on the <tabpanel> element
    TabPanel.setAttribute("index", 1);
    
    // Trigger setting of style for the tab widgets
    CellTab.setAttribute("selected", "true");
    TableTab.removeAttribute("selected");

    // Start editing on the cell element
    globalElement = cellGlobalElement;
  }
  else
  {
    // Start editing on the table element
    globalElement = cellGlobalElement;
  }

  if(!CellElement)
  {
    // Disable the Cell Properties tab -- only allow table props
    CellTab.setAttribute("disabled", "true");
  }
  
  doSetOKCancel(onOK, null);

  // Get widgets
  dialog.BorderWidthCheck = document.getElementById("BorderWidthCheck");
  dialog.BorderWidthInput = document.getElementById("BorderWidthInput");
  
    
  // This uses values set on globalElement
  InitDialog();

  // SET FOCUS TO FIRST CONTROL
}


function InitDialog()
{
  dump("Table Editing:InitDialog()\n");
}

function SelectTableTab()
{
  globalElement = tableGlobalElement;
}

function SelectCellTab()
{
  globalElement = cellGlobalElement;
}

function GetColorAndUpdate(ColorPickerID, ColorWellID, widget)
{
  // Close the colorpicker
  widget.parentNode.closePopup();
  SetColor(ColorWellID, getColor(ColorPickerID));
}

function SetColor(ColorWellID, color)
{
  // Save the color
  if (ColorWellID == "cellBackgroundCW")
    dialog.cellBackgroundColor = color;
  else
    dialog.tableBackgroundColor = color;
    
  setColorWell(ColorWellID, color); 
}

function ValidateData()
{
  dump("Table Editing:ValidateData()\n");
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    editorShell.CloneAttributes(TableElement, globalElement);
    return true;
  }
  return false;
}
