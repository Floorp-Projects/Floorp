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

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // GET EACH CONTROL -- E.G.:
  //dialog.editBox = document.getElementById("editBox");

  initDialog();
  
  // SET FOCUS TO FIRST CONTROL
  //SetTextboxFocus(dialog.editBox);
  SetWindowLocation();
}

function InitDialog() {
  // Get a single selected element of the desired type
  element = editorShell.GetSelectedElement(tagName);

  if (element) {
    // We found an element and don't need to insert one
    insertNew = false;
    dump("Found existing image\n");
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    element = editorShell.createElementWithDefaults(tagName);
  }

  if(!element)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
}

function onOK()
{
// Set attribute example:
//  imageElement.setAttribute("src",dialog.srcInput.value);
  if (insertNew) {
    try {
      editorShell.InsertElementAtSelection(element, false);
    } catch (e) {
      dump("Exception occured in InsertElementAtSelection\n");
    }
  }
  SaveWindowLocation();
  return true; // do close the window
}
