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
 * The Original Code is Button Properties Dialog.
 *
 * The Initial Developer of the Original Code is
 * Neil Rashbrook.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Neil Rashbrook <neil@parkwaycc.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var insertNew;
var buttonElement;

// dialog initialization code

function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }

  gDialog = {
    buttonType:       document.getElementById("ButtonType"),
    buttonName:       document.getElementById("ButtonName"),
    buttonValue:      document.getElementById("ButtonValue"),
    buttonDisabled:   document.getElementById("ButtonDisabled"),
    buttonTabIndex:   document.getElementById("ButtonTabIndex"),
    buttonAccessKey:  document.getElementById("ButtonAccessKey"),
    MoreSection:      document.getElementById("MoreSection"),
    MoreFewerButton:  document.getElementById("MoreFewerButton"),
    RemoveButton:     document.getElementById("RemoveButton")
  };

  // Get a single selected button element
  const kTagName = "button";
  try {
    buttonElement = editor.getSelectedElement(kTagName);
  } catch (e) {}

  if (buttonElement)
    // We found an element and don't need to insert one
    insertNew = false;
  else
  {
    insertNew = true;

    // We don't have an element selected,
    //  so create one with default attributes
    try {
      buttonElement = editor.createElementWithDefaults(kTagName);
    } catch (e) {}

    if (!buttonElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
    }
    // Hide button removing existing button
    gDialog.RemoveButton.hidden = true;
  }

  // Make a copy to use for AdvancedEdit
  globalElement = buttonElement.cloneNode(false);

  InitDialog();

  InitMoreFewer();

  gDialog.buttonType.focus();

  SetWindowLocation();
}

function InitDialog()
{
  var type = globalElement.getAttribute("type");
  var index = 0;
  switch (type)
  {
    case "button":
      index = 2;
      break;
    case "reset":
      index = 1;
      break;
  }
  gDialog.buttonType.selectedIndex = index;
  gDialog.buttonName.value = globalElement.getAttribute("name");
  gDialog.buttonValue.value = globalElement.getAttribute("value");
  gDialog.buttonDisabled.setAttribute("checked", globalElement.hasAttribute("disabled"));
  gDialog.buttonTabIndex.value = globalElement.getAttribute("tabindex");
  gDialog.buttonAccessKey.value = globalElement.getAttribute("accesskey");
}

function RemoveButton()
{
  RemoveContainer(buttonElement);
  SaveWindowLocation();
  window.close();
}

function ValidateData()
{
  var attributes = {
    type: ["", "reset", "button"][gDialog.buttonType.selectedIndex],
    name: gDialog.buttonName.value,
    value: gDialog.buttonValue.value,
    tabindex: gDialog.buttonTabIndex.value,
    accesskey: gDialog.buttonAccessKey.value
  };
  for (var a in attributes)
  {
    if (attributes[a])
      globalElement.setAttribute(a, attributes[a]);
    else
      globalElement.removeAttribute(a);
  }
  if (gDialog.buttonDisabled.checked)
    globalElement.setAttribute("disabled", "");
  else
    globalElement.removeAttribute("disabled");
  return true;
}

function onAccept()
{
  // All values are valid - copy to actual element in doc or
  //   element created to insert
  ValidateData();

  var editor = GetCurrentEditor();

  editor.cloneAttributes(buttonElement, globalElement);

  if (insertNew)
  {
    if (!InsertElementAroundSelection(buttonElement))
    {
      buttonElement.innerHTML = editor.outputToString("text/html", 1); // OutputSelectionOnly (see nsIDocumentEncoder.h)
      editor.insertElementAtSelection(buttonElement, true);
    }
  }

  SaveWindowLocation();

  return true;
}

