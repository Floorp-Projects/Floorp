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
// Note: This dialog
var tagname;
var element;

// dialog initialization code
function Startup()
{
  // This is the return value for the parent,
  //  who only needs to know if OK was clicked
  window.opener.AdvancedEditOK = false;

  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  // Element to edit is passed in
  element = window.arguments[1];
  if (!element || element == "undefined") {
    dump("Advanced Edit: Element to edit not supplied\n");
    window.close();  
  }
  dump("*** Element passed into Advanced Edit: "+element+" ***\n");

  // Append tagname of element we are editing after the message text
  //  above the attribute editing treewidget
  var msgParent = document.getElementById("AttributeMsgParent");

  // Remove temporary place holder text:
  // TODO: REMOVE THIS WHEN WE CAN RESIZE DIALOG AFTER CREATION
  msgParent.removeChild(msgParent.firstChild);

  var msg = editorShell.GetString("AttributesFor");
  dump("Tagname Msg = "+msg+"\n");
  msg +=(" "+element.nodeName);
  dump("Tagname Msg = "+msg+"\n");

  textNode = editorShell.editorDocument.createTextNode(msg);
  if (textNode) {
    msgParent.appendChild(textNode);
  }


  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.AddAttributeNameInput = document.getElementById("AddAttributeNameInput");
  //TODO: We should disable this button if the AddAttributeNameInput editbox is empty
  dialog.AddAttribute = document.getElementById("AddAttribute");

  //TODO: Get the list of attribute nodes,
  //  read each one, and use to build a text + editbox
  //  in a treewidget row for each attribute 
  var nodeMap = element.attributes;
  var nodeMapCount = nodeMap.length;

  // SET FOCUS TO FIRST CONTROL
}

function onAddAttribute()
{
  var name = dialog.AddAttributeNameInput.value;
  // TODO: Add a new text + editbox to the treewidget editing list
}


function onOK()
{
  //TODO: Get all children of the treewidget to get all
  //   name, value strings for all attributes.
  //   Set those attributes on "element" we are editing.

  window.opener.AdvancedEditOK = true;
  return true; // do close the window
}
