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

var gPrefs = GetPrefs();
var gEditor;

// dialog initialization code
function Startup()
{
  gEditor = GetCurrentEditor();
  if (!gEditor)
  {
    window.close();
    return;
  }

  gEditor instanceof Components.interfaces.nsIHTMLAbsPosEditor;

  gDialog.enableSnapToGrid = document.getElementById("enableSnapToGrid");
  gDialog.sizeInput        = document.getElementById("size");
  gDialog.sizeLabel        = document.getElementById("sizeLabel");
  gDialog.unitLabel        = document.getElementById("unitLabel");

  // Initialize control values based on existing attributes
  InitDialog()

  // SET FOCUS TO FIRST CONTROL
  SetTextboxFocus(gDialog.sizeInput);

  // Resize window
  window.sizeToContent();

  SetWindowLocation();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{
  gDialog.enableSnapToGrid.checked = gEditor.snapToGridEnabled;
  toggleSnapToGrid();

  gDialog.sizeInput.value = gEditor.gridSize;
}

function onAccept()
{
  gEditor.snapToGridEnabled = gDialog.enableSnapToGrid.checked;
  gEditor.gridSize = gDialog.sizeInput.value;

  return true;
}

function toggleSnapToGrid()
{
  SetElementEnabledById("size", gDialog.enableSnapToGrid.checked)
  SetElementEnabledById("sizeLabel", gDialog.enableSnapToGrid.checked)
  SetElementEnabledById("unitLabel", gDialog.enableSnapToGrid.checked)
}
