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
 */

//Cancel() is in EdDialogCommon.js
var tagName = "table"
var tableElement = null;
var rowElement = null;
var cellElement = null;
var maxRows = 10000;
var maxColumns = 10000;
var percentChar = "";
var maxPixels = 10000;
var rows;
var columns;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  dump(tagName+" = InsertTable tagName\n");
  tableElement = editorShell.CreateElementWithDefaults(tagName);
  if(!tableElement)
  {
    dump("Failed to create a new table!\n");
    window.close();
  }

  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.rowsInput = document.getElementById("rows");
  dialog.columnsInput = document.getElementById("columns");
  dialog.widthInput = document.getElementById("width");
  dialog.borderInput = document.getElementById("border");

  // Make a copy to use for AdvancedEdit
  globalElement = tableElement.cloneNode(false);
  
  // Initialize all widgets with image attributes
  InitDialog();

  dialog.rowsInput.focus();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitData()
{  
  // Get default attributes set on the created table:
  // Get the width attribute of the element, stripping out "%"
  // This sets contents of button text and "percentChar" variable
  dialog.widthInput.value = InitPixelOrPercentPopupButton(globalElement, "width", "pixelOrPercentButton");
  dialog.borderInput.value = globalElement.getAttribute("border");

  // Set default number to 1 row, 2 columns:
  dialog.rowsInput.value = 1;
  dialog.columnsInput.value = 2;
}

// Get and validate data from widgets.
// Set attributes on globalElement so they can be accessed by AdvancedEdit()
function ValidateData()
{
  rows = ValidateNumberString(dialog.rowsInput.value, 1, maxRows);
  if (rows == "") {
    // Set focus to the offending control
    dialog.rowsInput.focus();
    return false;
  }

  columns = ValidateNumberString(dialog.columnsInput.value, 1, maxColumns);
  if (columns == "") {
    // Set focus to the offending control
    dialog.columnsInput.focus();
    return false;
  }

  // Set attributes: these may be empty strings
  borderText = TrimString(dialog.borderInput.value);
  if (StringExists(borderText)) {
    // Set the other attributes on the table
    if (ValidateNumberString(borderText, 0, maxPixels))
      globalElement.setAttribute("border", borderText);
  }

  widthText = TrimString(dialog.widthInput.value);
  if (StringExists(widthText)) {
    var maxLimit;
    if (percentChar == "%") {
      maxLimit = 100;
    } else {
      // Upper limit when using pixels
      maxLimit = maxPixels;
    }

    widthText = ValidateNumberString(dialog.widthInput.value, 1, maxLimit);
    if (widthText != "") {
      widthText += percentChar;
      dump("Table Width="+widthText+"\n");
      globalElement.setAttribute("width", widthText);
    }
  }
  return true;
}


function onOK()
{
  if (ValidateData())
  {
    editorShell.CloneAttributes(tableElement, globalElement);

    // Create necessary rows and cells for the table
    dump("Rows = "+rows+"  Columns = "+columns+"\n");
    for (i = 0; i < rows; i++)
    {
      newRow = editorShell.CreateElementWithDefaults("tr");
      if (newRow)
      {
        tableElement.appendChild(newRow);
        for (j = 0; j < columns; j++)
        {
          newCell = editorShell.CreateElementWithDefaults("td");
          if (newCell)
          {
            newRow.appendChild(newCell);
          }
        }
      }
    }
  }

  // Don't delete selected text when inserting
  editorShell.InsertElement(tableElement, false);

  return true;
}
