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
var insertNew = true;
var tagname = "TAG NAME"

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  // gDialog is declared in EdDialogCommon.js
  // Set commonly-used widgets like this:
  gDialog.fooButton = document.getElementById("fooButton");

  initDialog();
  
  // Set window location relative to parent window (based on persisted attributes)
  SetWindowLocation();

  // Set focus to first widget in dialog, e.g.:
  SetTextboxFocus(gDialog.fooButton);
}

function InitDialog() 
{
  // Initialize all dialog widgets here,
  // e.g., get attributes from an element for property dialog
}

function onOK()
{
  // Validate all user data and set attributes and possibly insert new element here
  // If there's an error the user must correct, return false to keep dialog open.
  
  SaveWindowLocation();
  return true; // do close the window
}
