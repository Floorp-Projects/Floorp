/*  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

var insertNew = true;
var tagName = "anchor";
var anchorElement = null;
var nameInput;
var originalName = "";

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  nameInput = document.getElementById("nameInput");

  // Get a single selected element of the desired type
  anchorElement = editorShell.GetSelectedElement(tagName);

  if (anchorElement) {
    // We found an element and don't need to insert one
    insertNew = false;

    // Make a copy to use for AdvancedEdit
    globalElement = anchorElement.cloneNode(false);
    originalName = ConvertToCDATAString(anchorElement.name);
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    anchorElement = editorShell.CreateElementWithDefaults(tagName);
    if (anchorElement) {
      // Use the current selection as suggested name
      var name = GetSelectionAsText();
      // Get 40 characters of the selected text and don't add "...",
      //  replace whitespace with "_" and strip non-word characters
      name = ConvertToCDATAString(TruncateStringAtWordEnd(name, 40, false));
      //Be sure the name is unique to the document
      if (AnchorNameExists(name))
        name += "_"

      // Make a copy to use for AdvancedEdit
      globalElement = anchorElement.cloneNode(false);
      globalElement.setAttribute("name",name);
    }
  }
  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
    return;
  }

  InitDialog();
  
  DoEnabling();
  SetTextboxFocus(nameInput);
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
    //  have to UnEscapeAndConvert beforehand - too messy!
    nameInput.value = ConvertToCDATAString(nameInput.value);
  }
  DoEnabling();
}

function DoEnabling()
{
  SetElementEnabledById( "ok", nameInput.value.length > 0 );
}

function AnchorNameExists(name)
{
  var anchorList = editorShell.editorDocument.anchors;
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
  if (!name)
  {
      ShowInputErrorMessage(GetString("MissingAnchorNameError"));
      SetTextboxFocus(nameInput);
      return false;
  } else {
    // Replace spaces with "_" and strip other characters
    // Note: we could use ConvertAndEscape, but then we'd
    //  have to UnConverAndEscape beforehand - too messy!
    name = ConvertToCDATAString(name);

    if (originalName != name && AnchorNameExists(name))
    {
      ShowInputErrorMessage(GetString("DuplicateAnchorNameError").replace(/%name%/,name));            
      SetTextboxFocus(nameInput);
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
    if (originalName != globalElement.name)
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
    }
    SaveWindowLocation();
    return true;
  }
  return false;
}
