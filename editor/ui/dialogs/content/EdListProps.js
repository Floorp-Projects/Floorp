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

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  gDialog.ListTypeList = document.getElementById("ListType");
  gDialog.BulletStyleList = document.getElementById("BulletStyle");
  gDialog.BulletStyleLabel = document.getElementById("BulletStyleLabel");
  gDialog.StartingNumberInput = document.getElementById("StartingNumber");
  gDialog.StartingNumberLabel = document.getElementById("StartingNumberLabel");
  gDialog.AdvancedEditButton = document.getElementById("AdvancedEditButton1");
  gDialog.RadioGroup = document.getElementById("RadioGroup");
  gDialog.ChangeAllRadio = document.getElementById("ChangeAll");
  gDialog.ChangeSelectedRadio = document.getElementById("ChangeSelected");
  
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
  gDialog.RadioGroup.setAttribute("collapsed", ListElement ? "false" : "true");
  if (ListElement)
  {
    // Radio button index is persistant
    if (gDialog.RadioGroup.getAttribute("index") == "1")
      gDialog.RadioGroup.selectedItem = gDialog.ChangeSelectedRadio;
    else
      gDialog.RadioGroup.selectedItem = gDialog.ChangeAllRadio;
  }

  InitDialog();

  originalListType = ListType;

  gDialog.ListTypeList.focus();

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
  gDialog.StartingNumberInput.value = "";

  var type = globalElement ? globalElement.getAttribute("type") : null;

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
    gDialog.StartingNumberInput.value = globalElement.getAttribute("start");
  }
  gDialog.BulletStyleList.selectedIndex = index;
}

function BuildBulletStyleList()
{
  ClearMenulist(gDialog.BulletStyleList);
  var label;

  if (ListType == "ul")
  {
    gDialog.BulletStyleList.removeAttribute("disabled");
    gDialog.BulletStyleLabel.removeAttribute("disabled");
    gDialog.StartingNumberInput.setAttribute("disabled", "true");
    gDialog.StartingNumberLabel.setAttribute("disabled", "true");

    label = GetString("BulletStyle");

    AppendStringToMenulistById(gDialog.BulletStyleList,"Automatic");
    AppendStringToMenulistById(gDialog.BulletStyleList,"SolidCircle");
    AppendStringToMenulistById(gDialog.BulletStyleList,"OpenCircle");
    AppendStringToMenulistById(gDialog.BulletStyleList,"SolidSquare");

    gDialog.BulletStyleList.selectedIndex = BulletStyleIndex;
    gDialog.ListTypeList.selectedIndex = 1;
  }
  else if (ListType == "ol")
  {
    gDialog.BulletStyleList.removeAttribute("disabled");
    gDialog.BulletStyleLabel.removeAttribute("disabled");
    gDialog.StartingNumberInput.removeAttribute("disabled");
    gDialog.StartingNumberLabel.removeAttribute("disabled");
    label = GetString("NumberStyle");

    AppendStringToMenulistById(gDialog.BulletStyleList,"Automatic");
    AppendStringToMenulistById(gDialog.BulletStyleList,"Style_1");
    AppendStringToMenulistById(gDialog.BulletStyleList,"Style_I");
    AppendStringToMenulistById(gDialog.BulletStyleList,"Style_i");
    AppendStringToMenulistById(gDialog.BulletStyleList,"Style_A");
    AppendStringToMenulistById(gDialog.BulletStyleList,"Style_a");

    gDialog.BulletStyleList.selectedIndex = NumberStyleIndex;
    gDialog.ListTypeList.selectedIndex = 2;
  } 
  else 
  {
    gDialog.BulletStyleList.setAttribute("disabled", "true");
    gDialog.BulletStyleLabel.setAttribute("disabled", "true");
    gDialog.StartingNumberInput.setAttribute("disabled", "true");
    gDialog.StartingNumberLabel.setAttribute("disabled", "true");

    if (ListType == "dl")
      gDialog.ListTypeList.selectedIndex = 3;
    else
    {
      // No list or mixed selection that starts outside a list
      // ??? Setting index to 0 fails to draw menulist correctly!
      gDialog.ListTypeList.selectedIndex = 1;
      gDialog.ListTypeList.selectedIndex = 0;
    }
  }
  
  // Disable advanced edit button if changing to "normal"
  if (ListType)
    gDialog.AdvancedEditButton.removeAttribute("disabled");
  else
    gDialog.AdvancedEditButton.setAttribute("disabled","true");

  if (label)
    gDialog.BulletStyleLabel.setAttribute("value",label);
}

function SelectListType()
{
  var NewType;
  switch (gDialog.ListTypeList.selectedIndex)
  {
    case 1:
      NewType = "ul";
      break;
    case 2:
      NewType = "ol";
      SetTextboxFocus(gDialog.StartingNumberInput);
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
    BulletStyleIndex = gDialog.BulletStyleList.selectedIndex;
  else if (ListType == "ol")
  {
    var index = gDialog.BulletStyleList.selectedIndex;
    if (NumberStyleIndex != index)
    {
      NumberStyleIndex = index;
      SetTextboxFocus(gDialog.StartingNumberInput);
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
      switch (gDialog.BulletStyleList.selectedIndex)
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
      switch (gDialog.BulletStyleList.selectedIndex)
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
        
      var startingNumber = TrimString(gDialog.StartingNumberInput.value);
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


    // Remember which radio button was selected
    if (ListElement)
      gDialog.RadioGroup.setAttribute("index", gDialog.ChangeAllRadio.selected ? "0" : "1");

    var changeList;
    if (ListElement && gDialog.ChangeAllRadio.selected)
    {
      changeList = true;
    }
    else
      changeList = MixedListSelection || ListType != originalListType;

    if (changeList)
    {
      editorShell.MakeOrChangeList(ListType, gDialog.ChangeAllRadio.selected);

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
