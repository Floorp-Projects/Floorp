/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gInsertNew = true;
var gAnchorElement = null;
var gOriginalName = "";
const kTagName = "anchor";

// dialog initialization code
function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }

  gDialog.OkButton  = document.documentElement.getButton("accept");
  gDialog.NameInput = document.getElementById("nameInput");

  // Get a single selected element of the desired type
  gAnchorElement = editor.getSelectedElement(kTagName);

  if (gAnchorElement) {
    // We found an element and don't need to insert one
    gInsertNew = false;

    // Make a copy to use for AdvancedEdit
    globalElement = gAnchorElement.cloneNode(false);
    gOriginalName = ConvertToCDATAString(gAnchorElement.name);
  } else {
    gInsertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    gAnchorElement = editor.createElementWithDefaults(kTagName);
    if (gAnchorElement) {
      // Use the current selection as suggested name
      var name = GetSelectionAsText();
      // Get 40 characters of the selected text and don't add "...",
      //  replace whitespace with "_" and strip non-word characters
      name = ConvertToCDATAString(TruncateStringAtWordEnd(name, 40, false));
      //Be sure the name is unique to the document
      if (AnchorNameExists(name))
        name += "_"

      // Make a copy to use for AdvancedEdit
      globalElement = gAnchorElement.cloneNode(false);
      globalElement.setAttribute("name",name);
    }
  }
  if(!gAnchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
    return;
  }

  InitDialog();
  
  DoEnabling();
  SetTextboxFocus(gDialog.NameInput);
  SetWindowLocation();
}

function InitDialog()
{
  gDialog.NameInput.value = globalElement.getAttribute("name");
}

function ChangeName()
{
  if (gDialog.NameInput.value.length > 0)
  {
    // Replace spaces with "_" and strip other non-URL characters
    // Note: we could use ConvertAndEscape, but then we'd
    //  have to UnEscapeAndConvert beforehand - too messy!
    gDialog.NameInput.value = ConvertToCDATAString(gDialog.NameInput.value);
  }
  DoEnabling();
}

function DoEnabling()
{
  var enable = gDialog.NameInput.value.length > 0;
  SetElementEnabled(gDialog.OkButton,  enable);
  SetElementEnabledById("AdvancedEditButton1", enable);
}

function AnchorNameExists(name)
{
  var anchorList;
  try {
    anchorList = GetCurrentEditor().document.anchors;
  } catch (e) {}

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
  var name = TrimString(gDialog.NameInput.value);
  if (!name)
  {
      ShowInputErrorMessage(GetString("MissingAnchorNameError"));
      SetTextboxFocus(gDialog.NameInput);
      return false;
  } else {
    // Replace spaces with "_" and strip other characters
    // Note: we could use ConvertAndEscape, but then we'd
    //  have to UnConverAndEscape beforehand - too messy!
    name = ConvertToCDATAString(name);

    if (gOriginalName != name && AnchorNameExists(name))
    {
      ShowInputErrorMessage(GetString("DuplicateAnchorNameError").replace(/%name%/,name));            
      SetTextboxFocus(gDialog.NameInput);
      return false;
    }
    globalElement.name = name;
  }
  return true;
}

function onAccept()
{
  if (ValidateData())
  {
    if (gOriginalName != globalElement.name)
    {
      var editor = GetCurrentEditor();
      editor.beginTransaction();

      try {
        // "false" = don't delete selected text when inserting
        if (gInsertNew)
        {
          // We must insert element before copying CSS style attribute,
          //  but we must set the name else it won't insert at all
          gAnchorElement.name = globalElement.name;
          editor.insertElementAtSelection(gAnchorElement, false);
        }

        // Copy attributes to element we are changing or inserting
        editor.cloneAttributes(gAnchorElement, globalElement);

      } catch (e) {}

      editor.endTransaction();
    }
    SaveWindowLocation();
    return true;
  }
  return false;
}
