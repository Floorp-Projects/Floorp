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
var tagname = "TAG NAME"
var BulletStyleIndex = 0;
var NumberStyleIndex = 0;
var ListElement;
var originalListType = "";
var ListType = "";
var MixedListSelection = false;
var dialog;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  dialog = new Object;
  if (!dialog) {
    window.close();
    return;
  }
  doSetOKCancel(onOK, onCancel);

  dialog.ListTypeList = document.getElementById("ListType");
  dialog.BulletStyleList = document.getElementById("BulletStyle");
  dialog.BulletStyleLabel = document.getElementById("BulletStyleLabel");
  dialog.StartingNumberInput = document.getElementById("StartingNumber");
  dialog.StartingNumberLabel = document.getElementById("StartingNumberLabel");
  dialog.AdvancedEditButton = document.getElementById("AdvancedEditButton1");
  dialog.RadioGroup = document.getElementById("RadioGroup");
  dialog.ChangeAllRadio = document.getElementById("ChangeAll");
  dialog.ChangeSelectedRadio = document.getElementById("ChangeSelected");
  
  // Try to get an existing list(s)
  var mixedObj = new Object;
  ListType = editorShell.GetListState(mixedObj);
  // We may have mixed list and non-list, or > 1 list type in selection
  MixedListSelection = mixedObj.value;

  // Get the list element at the anchor node
  ListElement = editorShell.GetElementOrParentByTagName("list", null);

  // The copy to use in AdvancedEdit
  if (ListElement)
    globalElement = ListElement.cloneNode(false);

  // Show extra options for changing entire list if we have one already.
  dialog.RadioGroup.setAttribute("collapsed", ListElement ? "false" : "true");
  if (ListElement)
  {
    // Radio button index is persistant
    if (dialog.RadioGroup.getAttribute("index") == "1")
      dialog.ChangeSelectedRadio.checked = true;
    else
      dialog.ChangeAllRadio.checked = true;
  }

  InitDialog();

  originalListType = ListType;

  dialog.ListTypeList.focus();

  SetWindowLocation();
}

function InitDialog()
{
  // Note that if mixed, we we pay attention 
  //   only to the anchor node's list type
  // (i.e., don't confuse user with "mixed" designation)
  if (ListElement)
    ListType = ListElement.nodeName.toLowerCase();
  else
    ListType = "";
  
  BuildBulletStyleList();
  dialog.StartingNumberInput.value = "";

  var type = globalElement.getAttribute("type");

  var index = 0;
  if (ListType == "ul")
  {
    if (type)
    {
      type = type.toLowerCase();
      if (type == "disc")
        index = 1;
      else if (type == "circle")
        index = 2;
      else if (type == "square")
        index = 3;
    }
  }
  else if (ListType == "ol")
  {
    switch (type)
    {
      case "1":
        index = 1;
        break;
      case "I":
        index = 2;
        break;
      case "i":
        index = 3;
        break;
      case "A":
        index = 4;
        break;
      case "a":
        index = 5;
        break;
    }
    dialog.StartingNumberInput.value = globalElement.getAttribute("start");
  }
  dialog.BulletStyleList.selectedIndex = index;
}

function BuildBulletStyleList()
{
  ClearMenulist(dialog.BulletStyleList);
  var label;

  if (ListType == "ul")
  {
    dialog.BulletStyleList.removeAttribute("disabled");
    dialog.BulletStyleLabel.removeAttribute("disabled");
    dialog.StartingNumberInput.setAttribute("disabled", "true");
    dialog.StartingNumberLabel.setAttribute("disabled", "true");

    label = GetString("BulletStyle");

    AppendStringToMenulistById(dialog.BulletStyleList,"Automatic");
    AppendStringToMenulistById(dialog.BulletStyleList,"SolidCircle");
    AppendStringToMenulistById(dialog.BulletStyleList,"OpenCircle");
    AppendStringToMenulistById(dialog.BulletStyleList,"SolidSquare");

    dialog.BulletStyleList.selectedIndex = BulletStyleIndex;
    dialog.ListTypeList.selectedIndex = 1;
  }
  else if (ListType == "ol")
  {
    dialog.BulletStyleList.removeAttribute("disabled");
    dialog.BulletStyleLabel.removeAttribute("disabled");
    dialog.StartingNumberInput.removeAttribute("disabled");
    dialog.StartingNumberLabel.removeAttribute("disabled");
    label = GetString("NumberStyle");

    AppendStringToMenulistById(dialog.BulletStyleList,"Automatic");
    AppendStringToMenulistById(dialog.BulletStyleList,"Style_1");
    AppendStringToMenulistById(dialog.BulletStyleList,"Style_I");
    AppendStringToMenulistById(dialog.BulletStyleList,"Style_i");
    AppendStringToMenulistById(dialog.BulletStyleList,"Style_A");
    AppendStringToMenulistById(dialog.BulletStyleList,"Style_a");

    dialog.BulletStyleList.selectedIndex = NumberStyleIndex;
    dialog.ListTypeList.selectedIndex = 2;
  } 
  else 
  {
    dialog.BulletStyleList.setAttribute("disabled", "true");
    dialog.BulletStyleLabel.setAttribute("disabled", "true");
    dialog.StartingNumberInput.setAttribute("disabled", "true");
    dialog.StartingNumberLabel.setAttribute("disabled", "true");

    if (ListType == "dl")
      dialog.ListTypeList.selectedIndex = 3;
    else
    {
      // No list or mixed selection that starts outside a list
      // ??? Setting index to 0 fails to draw menulist correctly!
      dialog.ListTypeList.selectedIndex = 1;
      dialog.ListTypeList.selectedIndex = 0;
    }
  }
  
  // Disable advanced edit button if changing to "normal"
  if (ListType)
    dialog.AdvancedEditButton.removeAttribute("disabled");
  else
    dialog.AdvancedEditButton.setAttribute("disabled","true");

  if (label)
    dialog.BulletStyleLabel.setAttribute("value",label);
}

function SelectListType()
{
  var NewType;
  switch (dialog.ListTypeList.selectedIndex)
  {
    case 1:
      NewType = "ul";
      break;
    case 2:
      NewType = "ol";
      SetTextboxFocus(dialog.StartingNumberInput);
      break;
    case 3:
      NewType = "dl";
      break;
    default:
      NewType = "";
      break;
  }
  if (ListType != NewType)
  {
    ListType = NewType;
    
    // Create a newlist object for Advanced Editing
    if (ListType)
      globalElement = editorShell.CreateElementWithDefaults(ListType);

    BuildBulletStyleList();
  }
}

function SelectBulletStyle()
{
  // Save the selected index so when user changes
  //   list style, restore index to associated list
  if (ListType == "ul")
    BulletStyleIndex = dialog.BulletStyleList.selectedIndex;
  else if (ListType == "ol")
  {
    var index = dialog.BulletStyleList.selectedIndex;
    if (NumberStyleIndex != index)
    {
      NumberStyleIndex = index;
      SetTextboxFocus(dialog.StartingNumberInput);
    }
  }
}

function ValidateData()
{
  var type = 0;
  // globalElement should already be of the correct type 

  if (globalElement)
  {
    if (ListType == "ul")
    {
      switch (dialog.BulletStyleList.selectedIndex)
      {
        // Index 0 = automatic, the default, so we don't set it explicitly
        case 1:
          type = "disc";
          break;
        case 2:
          type = "circle";
          break;
        case 3:
          type = "square";
          break;
      }
      if (type)
        globalElement.setAttribute("type",type);
      else
        globalElement.removeAttribute("type");

    } 
    else if (ListType == "ol")
    {
      switch (dialog.BulletStyleList.selectedIndex)
      {
        // Index 0 = automatic, the default, so we don't set it explicitly
        case 1:
          type = "1";
          break;
        case 2:
          type = "I";
          break;
        case 3:
          type = "i";
          break;
        case 4:
          type = "A";
          break;
        case 5:
          type = "a";
          break;
      }
      if (type)
        globalElement.setAttribute("type",type);
      else
        globalElement.removeAttribute("type");
        
      var startingNumber = dialog.StartingNumberInput.value.trimString();
      if (startingNumber)
        globalElement.setAttribute("start",startingNumber);
      else
        globalElement.removeAttribute("start");
    }
  }
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    // Coalesce into one undo transaction
    editorShell.BeginBatchChanges();


    // Remember which radio button was checked
    if (ListElement)
      dialog.RadioGroup.setAttribute("index", dialog.ChangeAllRadio.checked ? "0" : "1");

    var changeList;
    if (ListElement && dialog.ChangeAllRadio.checked)
    {
      changeList = true;
    }
    else
      changeList = MixedListSelection || ListType != originalListType;

    if (changeList)
    {
      editorShell.MakeOrChangeList(ListType, dialog.ChangeAllRadio.checked);

      if (ListType)
      {
        // Get the new list created:
        ListElement = editorShell.GetElementOrParentByTagName(ListType, null);
      }
      else
      {
        // We removed an existing list
        ListElement = null;
      }
    }

    // Set the new list attributes
    if (ListElement)
      editorShell.CloneAttributes(ListElement, globalElement);

    editorShell.EndBatchChanges();
    
    SaveWindowLocation();

    return true;
  }
  return false;
}
