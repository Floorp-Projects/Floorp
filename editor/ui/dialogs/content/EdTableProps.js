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
var tagname = "table"
var element;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // GET EACH CONTROL -- E.G.:
  //dialog.editBox = document.getElementById("editBox");

/*
  // Get the selected or enclosing table element
  if(!element)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
*/
  InitDialog();
  
  var table = editorShell.GetElementOrParentByTagName("table", null);
  if (!table)
    window.close();

  // SET FOCUS TO FIRST CONTROL
  //dialog.editBox.focus();

}

function InitDialog()
{
  dump{"Table Editing:InitDialog()\n");
}

function onAdvancedEdit()
{
  dump("\n\n Need to write onAdvancedEdit for Table and Cell dialog\n\n");
}

function onOK()
{
// Set attribute example:
//  imageElement.setAttribute("src",dialog.srcInput.value);
  return true;
}
