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
var maxPixels = 10000;
var rows;
var columns;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  tableElement = editorShell.CreateElementWithDefaults(tagName);
  if(!tableElement)
  {
    dump("Failed to create a new table!\n");
    window.close();
  }
  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.rowsInput    = document.getElementById("rowsInput");
  dialog.columnsInput = document.getElementById("columnsInput");
  dialog.widthInput = document.getElementById("widthInput");
  dialog.heightInput = document.getElementById("heightInput");
  dialog.borderInput = document.getElementById("borderInput");
  dialog.widthPixelOrPercentMenulist = document.getElementById("widthPixelOrPercentMenulist");
  dialog.heightPixelOrPercentMenulist = document.getElementById("heightPixelOrPercentMenulist");

  // Make a copy to use for AdvancedEdit
  globalElement = tableElement.cloneNode(false);
  
  // Initialize all widgets with image attributes
  InitDialog();

  // Set initial number to 2 rows, 2 columns:
  // Note, these are not attributes on the table,
  //  so don't put them in InitDialog(),
  //  else the user's values will be trashed when they use 
  //  the Advanced Edit dialog
  dialog.rowsInput.value = 2;
  dialog.columnsInput.value = 2;

  // If no default value on the width, set to 100%
  if (dialog.widthInput.value.length == 0)
  {
    dialog.widthInput.value = "100";
    dialog.widthPixelOrPercentMenulist.selectedIndex = 1;
  }

  // Resize window
  window.sizeToContent();

  SetTextfieldFocus(dialog.rowsInput);

  SetWindowLocation();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{  
  // Get default attributes set on the created table:
  // Get the width attribute of the element, stripping out "%"
  // This sets contents of menu combobox list
  dialog.widthInput.value = InitPixelOrPercentMenulist(globalElement, tableElement, "widthInput","widthPixelOrPercentMenulist", gPercent);
  dialog.heightInput.value = InitPixelOrPercentMenulist(globalElement, tableElement, "heightInput","heightPixelOrPercentMenulist", gPixel);
  dialog.borderInput.value = globalElement.getAttribute("border");
}

function ChangeRowOrColumn(id)
{
  // Allow only integers
  forceInteger(id);

  // Enable OK only if both rows and columns have a value > 0
  SetElementEnabledById("ok", dialog.rowsInput.value.length > 0 && 
                              dialog.rowsInput.value > 0 &&
                              dialog.columnsInput.value.length > 0 &&
                              dialog.columnsInput.value > 0);
}


// Get and validate data from widgets.
// Set attributes on globalElement so they can be accessed by AdvancedEdit()
function ValidateData()
{
  rows = ValidateNumberString(dialog.rowsInput.value, 1, maxRows);
  if (rows == "")
  {
    // Set focus to the offending control
    dialog.rowsInput.focus();
    return false;
  }

  columns = ValidateNumberString(dialog.columnsInput.value, 1, maxColumns);
  if (columns == "")
  {
    // Set focus to the offending control
    SetTextfieldFocus(dialog.columnsInput);
    return false;
  }

  // Set attributes: NOTE: These may be empty strings
  borderText = TrimString(dialog.borderInput.value);
  if (borderText) 
  {
    // Set the other attributes on the table
    if (ValidateNumberString(borderText, 0, maxPixels))
      globalElement.setAttribute("border", borderText);
  }

  var maxLimit;
  var isPercent = (dialog.widthPixelOrPercentMenulist.selectedIndex == 1);
  widthText = TrimString(dialog.widthInput.value);
  if (widthText.length > 0)
  {
    if (isPercent)
      maxLimit = 100;
    else
      // Upper limit when using pixels
      maxLimit = maxPixels;

    widthText = ValidateNumberString(widthText, 1, maxLimit);
    if (widthText.length == 0)
      return false;

    if (isPercent)
      widthText += "%";
    globalElement.setAttribute("width", widthText);
  }

  isPercent = (dialog.heightPixelOrPercentMenulist.selectedIndex == 1);
  heightText = TrimString(dialog.heightInput.value);
  if (heightText.length > 0)
  {
    if (isPercent)
      maxLimit = 100;
    else
      // Upper limit when using pixels
      maxLimit = maxPixels;

    heightText = ValidateNumberString(heightText, 1, maxLimit);
    if (heightText.length == 0)
      return false;

    if (isPercent)
      heightText += "%";

    globalElement.setAttribute("height", heightText);
  }
  return true;
}


function onOK()
{
  if (ValidateData())
  {
    editorShell.CloneAttributes(tableElement, globalElement);

    // Create necessary rows and cells for the table
    // AFTER BUG 30378 IS FIXED, DON'T INSERT TBODY!
    var tableBody = editorShell.CreateElementWithDefaults("tbody");
    if (tableBody)
    {
      tableElement.appendChild(tableBody);

      // Create necessary rows and cells for the table
      //dump("Rows = "+rows+"  Columns = "+columns+"\n");
      for (var i = 0; i < rows; i++)
      {
        var newRow = editorShell.CreateElementWithDefaults("tr");
        if (newRow)
        {
          tableBody.appendChild(newRow);
          for (var j = 0; j < columns; j++)
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
    try {
      // Don't delete selected text when inserting
      editorShell.InsertElementAtSelection(tableElement, false);
    } catch (e) {
      dump("Exception occured in InsertElementAtSelection\n");
    }
    SaveWindowLocation();
    return true;
  }
  return false;
}
