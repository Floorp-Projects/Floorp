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

  doSetOKCancel(onOK, onCancel);

  nameInput = document.getElementById("nameInput");

  dump("tagName = "+tagName+"\n");

  // Get a single selected element of the desired type
  anchorElement = editorShell.GetSelectedElement(tagName);

  if (anchorElement) {
    // We found an element and don't need to insert one
    insertNew = false;
    dump("Found existing anchor\n");

    // Make a copy to use for AdvancedEdit
    globalElement = anchorElement.cloneNode(false);
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    anchorElement = editorShell.CreateElementWithDefaults(tagName);
    if (anchorElement) {
      // Use the current selection as suggested name
      var name = GetSelectionAsText();
      // Get 40 characters of the selected text and don't add "...",
      //  replace whitespace with "_" and strip non-word characters
      name = PrepareStringForURL(TruncateStringAtWordEnd(name, 40, false));
      //Be sure the name is unique to the document
      if (AnchorNameExists(name))
        name += "_"

      // Make a copy to use for AdvancedEdit
      globalElement = anchorElement.cloneNode(false);
      globalElement.setAttribute("name",name);
      //dump(globalElement+name+" = name"+" Get name Attribute="+globalElement.getAttribute("name")+"\n");
    }
  }
  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }

  InitDialog();
  
  SetTextfieldFocus(nameInput);
  SetWindowLocation();
}

function InitDialog()
{
  nameInput.value = globalElement.getAttribute("name");
}

function ChangeName()
{
  if (nameInput.value.length > 0)
  {
    // Replace spaces with "_" and strip other non-URL characters
    // Note: we could use ConvertAndEscape, but then we'd
    //  have to UnConverAndEscape beforehand - too messy!
    nameInput.value = PrepareStringForURL(nameInput.value.replace(/\s+/g, "_"));
  }
}

function AnchorNameExists(name)
{
  anchorList = editorShell.editorDocument.anchors;
  if (anchorList) {
    for (var i = 0; i < anchorList.length; i++) {
      if (anchorList[i].name == name)
        return true;
    }
  }
  return false;
}

// Get and validate data from widgets.
// Set attributes on globalElement so they can be accessed by AdvancedEdit()
function ValidateData()
{
  var name = TrimString(nameInput.value);
  if (name.length == 0)
  {
      ShowInputErrorMessage(GetString("MissingAnchorNameError"));
      SetTextfieldFocus(nameInput);
      return false;
  } else {
    // Replace spaces with "_" and strip other characters
    // Note: we could use ConvertAndEscape, but then we'd
    //  have to UnConverAndEscape beforehand - too messy!
    name = PrepareStringForURL(name);

    if (AnchorNameExists(name))
    {
      ShowInputErrorMessage(GetString("DuplicateAnchorNameError").replace(/%name%/,name));            
      SetTextfieldFocus(nameInput);
      return false;
    }
    globalElement.setAttribute("name",name);
  }
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    // Copy attributes to element we are changing or inserting
    editorShell.CloneAttributes(anchorElement, globalElement);

    if (insertNew) {
      // Don't delete selected text when inserting
      try {
        editorShell.InsertElementAtSelection(anchorElement, false);
      } catch (e) {
        dump("Exception occured in InsertElementAtSelection\n");
      }
    }
    SaveWindowLocation();
    return true;
  }
  return false;
}
