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

var insertNew = true;
var tagName = "anchor";
var anchorElement = null;
var nameInput;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  nameInput = document.getElementById("nameInput");

  dump("tagName = "+tagName+"\n");

  // Get a single selected element of the desired type
  anchorElement = editorShell.GetSelectedElement(tagName);

  if (anchorElement) {
    // We found an element and don't need to insert one
    insertNew = false;
    dump("Found existing anchor\n");
    nameInput.value = anchorElement.getAttribute("name");
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    anchorElement = editorShell.CreateElementWithDefaults(tagName);
    // Use the current selection as suggested name
    name = GetSelectionAsText();
    // Get 40 characters of the selected text and don't add "..."
    name = TruncateStringAtWordEnd(name, 40, false);
    // Replace whitespace with "_"
    name = ReplaceWhitespace(name, "_");

    //Be sure the name is unique to the document
    if (AnchorNameExists(name))
      name += "_"

    nameInput.value = name;
  }

  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
  
  nameInput.focus();
}

function AnchorNameExists(name)
{
  anchorList = editorShell.editorDocument.anchors; // getElementsByTagName("A");
  if (anchorList) {
    dump("We have an anchor list\n");
    for (i=0; i < anchorList.length; i++) {
      dump("Anchor name: "+anchorList[i].name+"\n");
      if (anchorList[i].name == name)
        return true;
    }
  }
  return false;
}

function onOK()
{
  name = nameInput.value;
  name = TrimString(name);
  if (name.length == 0) {
      ShowInputErrorMessage("You must enter a name for this anchor.");
      nameInput.focus();
      return false;
  } else {
    // Replace spaces with "_" else it causes trouble in URL parsing
    name = ReplaceWhitespace(name, "_");
    if (AnchorNameExists(name)) {
      ShowInputErrorMessage("\""+name+"\" already exists in this page.\nPlease enter a different name.");            
      nameInput.focus();
      return false;
    }
    anchorElement.setAttribute("name",name);
    if (insertNew) {
      // Don't delete selected text when inserting
      editorShell.InsertElement(anchorElement, false);
    }
  }
  return true;
}
