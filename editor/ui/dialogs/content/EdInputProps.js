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
 * The Original Code is Input Tag Properties Dialog.
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
var inputElement;

// dialog initialization code

function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    dump("Failed to get active editor!\n");
    window.close();
    return;
  }

  gDialog = {
    accept:             document.documentElement.getButton("accept"),
    inputType:          document.getElementById("InputType"),
    inputNameDeck:      document.getElementById("InputNameDeck"),
    inputName:          document.getElementById("InputName"),
    inputValueDeck:     document.getElementById("InputValueDeck"),
    inputValue:         document.getElementById("InputValue"),
    inputDeck:          document.getElementById("InputDeck"),
    inputChecked:       document.getElementById("InputChecked"),
    inputSelected:      document.getElementById("InputSelected"),
    inputReadOnly:      document.getElementById("InputReadOnly"),
    inputDisabled:      document.getElementById("InputDisabled"),
    inputTabIndex:      document.getElementById("InputTabIndex"),
    inputAccessKey:     document.getElementById("InputAccessKey"),
    inputSize:          document.getElementById("InputSize"),
    inputMaxLength:     document.getElementById("InputMaxLength"),
    inputAccept:        document.getElementById("InputAccept"),
    MoreSection:        document.getElementById("MoreSection"),
    MoreFewerButton:    document.getElementById("MoreFewerButton"),
    AdvancedEditButton: document.getElementById("AdvancedEditButton"),
    AdvancedEditDeck:   document.getElementById("AdvancedEditDeck")
  };

  // Get a single selected input element
  const kTagName = "input";
  try {
    inputElement = editor.getSelectedElement(kTagName);
  } catch (e) {}

  if (inputElement)
    // We found an element and don't need to insert one
    insertNew = false;
  else
  {
    insertNew = true;

    // We don't have an element selected,
    //  so create one with default attributes
    try {
      inputElement = editor.createElementWithDefaults(kTagName);
    } catch (e) {}

    if (!inputElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
    }

    var imgElement = editor.getSelectedElement("img");
    if (imgElement)
    {
      // We found an image element, convert it to an input type="image"
      inputElement.setAttribute("type", "image");

      var attributes = ["src", "alt", "width", "height", "hspace", "vspace", "border", "align"];
      for (i in attributes)
        inputElement.setAttribute(attributes[i], imgElement.getAttribute(attributes[i]));
    }
    else
      inputElement.setAttribute("value", GetSelectionAsText());
  }

  // Make a copy to use for AdvancedEdit
  globalElement = inputElement.cloneNode(false);

  InitDialog();

  InitMoreFewer();

  gDialog.inputType.focus();

  SetWindowLocation();
}

function InitDialog()
{
  var type = globalElement.getAttribute("type");
  var index = 0;
  switch (type)
  {
    case "button":
      index = 9;
      break;
    case "checkbox":
      index = 2;
      break;
    case "file":
      index = 6;
      break;
    case "hidden":
      index = 7;
      break;
    case "image":
      index = 8;
      break;
    case "password":
      index = 1;
      break;
    case "radio":
      index = 3;
      break;
    case "reset":
      index = 5;
      break;
    case "submit":
      index = 4;
      break;
  }
  gDialog.inputType.selectedIndex = index;
  gDialog.inputName.value = globalElement.getAttribute("name");
  gDialog.inputValue.value = globalElement.getAttribute("value");
  gDialog.inputChecked.setAttribute("checked", globalElement.hasAttribute("checked"));
  gDialog.inputSelected.setAttribute("checked", globalElement.hasAttribute("checked"));
  gDialog.inputReadOnly.setAttribute("checked", globalElement.hasAttribute("readonly"));
  gDialog.inputDisabled.setAttribute("checked", globalElement.hasAttribute("disabled"));
  gDialog.inputTabIndex.value = globalElement.getAttribute("tabindex");
  gDialog.inputAccessKey.value = globalElement.getAttribute("accesskey");
  gDialog.inputSize.value = globalElement.getAttribute("size");
  gDialog.inputMaxLength.value = globalElement.getAttribute("maxlength");
  gDialog.inputAccept.value = globalElement.getAttribute("accept");
  SelectInputType();
}

function SelectInputType()
{
  var index = gDialog.inputType.selectedIndex;
  gDialog.AdvancedEditDeck.setAttribute("selectedIndex", 0);
  gDialog.inputNameDeck.setAttribute("selectedIndex", 0);
  gDialog.inputValueDeck.setAttribute("selectedIndex", 0);
  gDialog.inputValue.disabled = false;
  gDialog.inputChecked.disabled = index != 2;
  gDialog.inputSelected.disabled = index != 3;
  gDialog.inputReadOnly.disabled = index > 1;
  gDialog.inputTabIndex.disabled = index == 7;
  gDialog.inputAccessKey.disabled = index == 7;
  gDialog.inputSize.disabled = index > 1;
  gDialog.inputMaxLength.disabled = index > 1;
  gDialog.inputAccept.disabled = index != 6;
  switch (index)
  {
    case 0:
    case 1:
      gDialog.inputValueDeck.setAttribute("selectedIndex", 1);
      gDialog.inputDeck.setAttribute("selectedIndex", 2);
      break;
    case 2:
      gDialog.inputDeck.setAttribute("selectedIndex", 0);
      break;
    case 3:
      gDialog.inputDeck.setAttribute("selectedIndex", 1);
      gDialog.inputNameDeck.setAttribute("selectedIndex", 1);
      break;
    case 6:
      gDialog.inputValue.disabled = true;
      gDialog.inputAccept.disabled = false;
      break;
    case 8:
      gDialog.inputValue.disabled = true;
      gDialog.AdvancedEditDeck.setAttribute("selectedIndex", 1);
      gDialog.inputName.removeEventListener("input", onInput, false);
      break;
    case 7:
      gDialog.inputValueDeck.setAttribute("selectedIndex", 1);
      break;
  }
  onInput();
}

function onInput()
{
  var disabled = false;;
  switch (gDialog.inputType.selectedIndex)
  {
  case 3:
    disabled = disabled || !gDialog.inputValue.value;
  case 4:
  case 5:
    break;
  case 8:
    disabled = !globalElement.hasAttribute("src");
    break;
  default:
    disabled = !gDialog.inputName.value
    break;
  }
  if (gDialog.accept.disabled != disabled)
  {
    gDialog.accept.disabled = disabled;
    gDialog.AdvancedEditButton.disabled = disabled;
  }
}

function doImageProperties()
{
  window.openDialog("chrome://editor/content/EdImageProps.xul",
                    "_blank", "chrome,close,titlebar,modal", globalElement);
  window.focus();
  onInput();
}

function ValidateData()
{
  var attributes = {
    type: "",
    name: gDialog.inputName.value,
    value: gDialog.inputValue.value,
    tabindex: gDialog.inputTabIndex.value,
    accesskey: "",
    size: "",
    maxlength: "",
    accept: ""
  };
  var index = gDialog.inputType.selectedIndex;
  var flags = {
    checked: false,
    readonly: false,
    disabled: gDialog.inputDisabled.checked
  };
  switch (index)
  {
    case 1:
      attributes.type = "password";
    case 0:
      flags.readonly = gDialog.inputReadOnly.checked;
      attributes.size = gDialog.inputSize.value;
      attributes.maxlength = gDialog.inputMaxLength.value;
      break;
    case 2:
      attributes.type = "checkbox";
      flags.checked = gDialog.inputChecked.checked;
      break;
    case 3:
      attributes.type = "radio";
      flags.checked = gDialog.inputSelected.checked;
      break;
    case 4:
      attributes.type = "submit";
      attributes.accesskey = gDialog.inputAccessKey.value;
      break;
    case 5:
      attributes.type = "reset";
      attributes.accesskey = gDialog.inputAccessKey.value;
      break;
    case 6:
      attributes.type = "file";
      attributes.accept = gDialog.inputAccept.value;
      attributes.value = "";
      break;
    case 7:
      attributes.type = "hidden";
      attributes.tabindex = "";
      break;
    case 8:
      attributes.type = "image";
      attributes.value = "";
      break;
    case 9:
      attributes.type = "button";
      attributes.accesskey = gDialog.inputAccessKey.value;
      break;
  }
  for (var a in attributes)
  {
    if (attributes[a])
      globalElement.setAttribute(a, attributes[a]);
    else
      globalElement.removeAttribute(a);
  }
  for (var f in flags)
  {
    if (flags[f])
      globalElement.setAttribute(f, "");
    else
      globalElement.removeAttribute(f);
  }
  return true;
}

function onAccept()
{
  if (ValidateData())
  {
    // All values are valid - copy to actual element in doc or
    //   element created to insert

    var editor = GetCurrentEditor();

    editor.cloneAttributes(inputElement, globalElement);

    if (insertNew)
    {
      try {
        // 'true' means delete the selection before inserting
        // in case were are converting an image to an input type="image"
        editor.insertElementAtSelection(inputElement, true);
      } catch (e) {
        dump(e);
      }
    }

    SaveWindowLocation();

    return true;
  }
  return false;
}

