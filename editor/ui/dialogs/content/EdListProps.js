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
var ListTypeList;
var BulletStyleList;
var BulletStyleLabel;
var StartingNumberInput;
var StartingNumberLabel;
var BulletStyleIndex = 0;
var NumberStyleIndex = 0;
var ListElement = 0;
var originalListType = "";
var ListType = "";
var MixedListSelection = false;
var AdvancedEditButton;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  ListTypeList = document.getElementById("ListType");
  BulletStyleList = document.getElementById("BulletStyle");
  BulletStyleLabel = document.getElementById("BulletStyleLabel");
  StartingNumberInput = document.getElementById("StartingNumber");
  StartingNumberLabel = document.getElementById("StartingNumberLabel");
  AdvancedEditButton = document.getElementById("AdvancedEditButton1");
  
  // Try to get an existing list

  // This gets a single list element enclosing the entire selection
  ListElement = editorShell.GetSelectedElement("list");

  if (!ListElement)
  {
    // Get the list at the anchor node
    ListElement = editorShell.GetElementOrParentByTagName("list", null);

    // Remember if we have a list at anchor node, but focus node
    //   is not in a list or is a different type of list
    if (ListElement)
      MixedListSelection = true;
  }
  
  // The copy to use in AdvancedEdit
  if (ListElement)
    globalElement = ListElement.cloneNode(false);

  //dump("List and global elements: "+ListElement+globalElement+"\n");

  InitDialog();

  originalListType = ListType;

  ListTypeList.focus();

  SetWindowLocation();
}

function InitDialog()
{
  if (ListElement)
    ListType = ListElement.nodeName.toLowerCase();
  else
    ListType = "";
  
  BuildBulletStyleList();
}

function BuildBulletStyleList()
{
  ClearMenulist(BulletStyleList);
  var label = "";
  var selectedIndex = -1;

dump("List Type: "+ListType+" globalElement: "+globalElement+"\n");

  if (ListType == "ul")
  {
    BulletStyleList.removeAttribute("disabled");
    BulletStyleLabel.removeAttribute("disabled");
    StartingNumberInput.setAttribute("disabled", "true");
    StartingNumberLabel.setAttribute("disabled", "true");
    label = GetString("BulletStyle");

    AppendStringToMenulistById(BulletStyleList,"SolidCircle");
    AppendStringToMenulistById(BulletStyleList,"OpenCircle");
    AppendStringToMenulistById(BulletStyleList,"SolidSquare");

    BulletStyleList.selectedIndex = BulletStyleIndex;
    ListTypeList.selectedIndex = 1;
  }
  else if (ListType == "ol")
  {
    BulletStyleList.removeAttribute("disabled");
    BulletStyleLabel.removeAttribute("disabled");
    StartingNumberInput.removeAttribute("disabled");
    StartingNumberLabel.removeAttribute("disabled");
    label = GetString("NumberStyle");

    AppendStringToMenulistById(BulletStyleList,"Style_1");
    AppendStringToMenulistById(BulletStyleList,"Style_I");
    AppendStringToMenulistById(BulletStyleList,"Style_i");
    AppendStringToMenulistById(BulletStyleList,"Style_A");
    AppendStringToMenulistById(BulletStyleList,"Style_a");

    BulletStyleList.selectedIndex = NumberStyleIndex;
    ListTypeList.selectedIndex = 2;
  } 
  else 
  {
    BulletStyleList.setAttribute("disabled", "true");
    BulletStyleLabel.setAttribute("disabled", "true");
    StartingNumberInput.setAttribute("disabled", "true");
    StartingNumberLabel.setAttribute("disabled", "true");

    if (ListType == "dl")
      ListTypeList.selectedIndex = 3;
    else
      ListTypeList.selectedIndex = 0;
  }
  
  // Disable advanced edit button if changing to "normal"
  if (ListType == "")
    AdvancedEditButton.setAttribute("disabled","true");
  else
    AdvancedEditButton.removeAttribute("disabled");

  if (label)
    BulletStyleLabel.setAttribute("value",label);
}

function SelectListType()
{
//dump(ListTypeList+"ListTypeList\n");
  switch (ListTypeList.selectedIndex)
  {
    case 1:
      NewType = "ul";
      break;
    case 2:
      NewType = "ol";
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
    if (ListType != "")
      globalElement = editorShell.CreateElementWithDefaults(ListType);

    BuildBulletStyleList();
  }
}

function SelectBulletStyle()
{
  // Save the selected index so when user changes
  //   list style, restore index to associated list
  if (ListType == "ul")
    BulletStyleIndex = BulletStyleList.selectedIndex;
  else if (ListType == "ol")
  {
    if (NumberStyleIndex != BulletStyleList.selectedIndex)
      StartingNumberInput.value = "";  
    NumberStyleIndex = BulletStyleList.selectedIndex;
  }
}

function FilterStartNumber()
{
  var stringIn = StartingNumberInput.value.trimString();
  if (stringIn.length > 0)
  {
    switch (NumberStyleIndex)
    {
      case 0:
        // Allow only integers
        stringIn = stringIn.replace(/\D+/g,"");
        break;
      case 1:  // "I";
        stringIn = stringIn.toUpperCase().replace(/[^ICDVXL]+/g,"");
        break;
      case 2:  // "i";
        stringIn = stringIn.toLowerCase().replace(/[^icdvxl]+/g,"");
        break;
      case 3:  // "A";
        stringIn.toUpperCase();
        break;
      case 4:  // "a";
        stringIn.toLowerCase();
        break;
    }
    if (!stringIn) stringIn = "";
    StartingNumberInput.value = stringIn;
  }
}

function ValidateData()
{
  var type = 0;
  // globalElement should already be of the correct type 
  //dump("Global List element="+globalElement+"  should be type: "+ListType+"\n");

  if (globalElement)
  {
    if (ListType == "ul")
    {
      switch (BulletStyleList.selectedIndex)
      {
        // Index 0 = "disc", the default, so we don't set it explicitly
        case 1:
          type = "circle";
          break;
        case 2:
          type = "square";
          break;
      }
      if (type)
        globalElement.setAttribute("type",type);
      else
        globalElement.removeAttribute("type");

    } else if (ListType == "ol")
    {
      switch (BulletStyleList.selectedIndex)
      {
        // Index 0 = "1", the default, so we don't set it explicitly
        case 1:
          type = "I";
          break;
        case 2:
          type = "i";
          break;
        case 3:
          type = "A";
          break;
        case 4:
          type = "a";
          break;
      }
      if (type)
        globalElement.setAttribute("type",type);
      else
        globalElement.removeAttribute("type");
        
      var startingNumber = StartingNumberInput.value.trimString();
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

    // We may need to create new list element(s)
    //   or change to a different list type.
    if (!ListElement || MixedListSelection || ListType != originalListType)
    {
      editorShell.MakeOrChangeList(ListType);
      // Get the new list created:
      //ListElement = editorShell.GetSelectedElement("list");
      ListElement = editorShell.GetElementOrParentByTagName(ListType, null);
    }
    // Set the list attributes
    if (ListElement)
      editorShell.CloneAttributes(ListElement, globalElement);

    editorShell.EndBatchChanges();
    
    SaveWindowLocation();
    return true;
  }
  return false;
}
