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
var tableElement;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  // Create dialog object to store controls for easy access
  // GET EACH CONTROL -- E.G.:
  //dialog.editBox = document.getElementById("editBox");

  var table = editorShell.GetElementOrParentByTagName(tagname, null);
  if(!tableElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }

  globalElement = tableElement.cloneNode(false);
  
  // This uses values set on globalElement
  InitDialog();

  // SET FOCUS TO FIRST CONTROL
 }


function InitDialog()
{
  dump{"Table Editing:InitDialog()\n");
}

function ValidateData()
{
  dump{"Table Editing:ValidateData()\n");
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    editorShell.CloneAttributes(tableElement, globalElement);
    return true;
  }
  return false;
}
