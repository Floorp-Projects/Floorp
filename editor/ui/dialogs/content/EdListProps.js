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
var insertNew = true;
var tagname = "TAG NAME"
var ListStyleList;
var BulletStyleList;
var BulletStyleLabel;
var StartingNumberInput;
var StartingNumberLabel;
var BulletStyleIndex = 0;
var NumberStyleIndex = 0;
var ListStyle = "";

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);

  ListStyleList = document.getElementById("ListStyle");
  BulletStyleList = document.getElementById("BulletStyle");
  BulletStyleLabel = document.getElementById("BulletStyleLabel");
  StartingNumberInput = document.getElementById("StartingNumber");
  StartingNumberLabel = document.getElementById("StartingNumberLabel");

  InitDialog();
  ListStyleList.focus();
}

function InitDialog()
{
  //TODO: Get the current list style and set in ListStyle variable
  ListStyle = "ul";
  BuildBulletStyleList();
/*
  if(!element)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
*/
}

function BuildBulletStyleList()
{
  // Put methods on the object if I can figure out its name!
  // (SELECT and HTMLSelect don't work!)
  //BulletStyleList.clear();
  
  ClearList(BulletStyleList);
  var label = "";
  var selectedIndex = -1;

  if (ListStyle == "ul")
  {
    BulletStyleList.removeAttribute("disabled");
    BulletStyleLabel.removeAttribute("disabled");
    StartingNumberInput.setAttribute("disabled", "true");
    StartingNumberLabel.setAttribute("disabled", "true");
    label = GetString("BulletStyle");

    AppendStringToListByID(BulletStyleList,"SolidCircle");
    AppendStringToListByID(BulletStyleList,"OpenCircle");
    AppendStringToListByID(BulletStyleList,"OpenSquare");

    BulletStyleList.selectedIndex = BulletStyleIndex;
    ListStyleList.selectedIndex = 1;
  } else if (ListStyle == "ol")
  {
    BulletStyleList.removeAttribute("disabled");
    BulletStyleLabel.removeAttribute("disabled");
    StartingNumberInput.removeAttribute("disabled");
    StartingNumberLabel.removeAttribute("disabled");
    label = GetString("NumberStyle");

    AppendStringToListByID(BulletStyleList,"Style_1");
    AppendStringToListByID(BulletStyleList,"Style_I");
    AppendStringToListByID(BulletStyleList,"Style_i");
    AppendStringToListByID(BulletStyleList,"Style_A");
    AppendStringToListByID(BulletStyleList,"Style_a");

    BulletStyleList.selectedIndex = NumberStyleIndex;
    ListStyleList.selectedIndex = 2;
  } else {
    BulletStyleList.setAttribute("disabled", "true");
    BulletStyleLabel.setAttribute("disabled", "true");
    StartingNumberInput.setAttribute("disabled", "true");
    StartingNumberLabel.setAttribute("disabled", "true");

    if (ListStyle == "dl")
      ListStyleList.selectedIndex = 3;
    else
      ListStyleList.selectedIndex = 0;
  }

  if (label)
  {
    if (BulletStyleLabel.hasChildNodes())
      BulletStyleLabel.removeChild(BulletStyleLabel.firstChild);
    
    var textNode = document.createTextNode(label);
    BulletStyleLabel.appendChild(textNode);
  }
}

function SelectListStyle()
{
  switch (ListStyleList.selectedIndex)
  {
    case 1:
      ListStyle = "ul";
      break;
    case 2:
      ListStyle = "ol";
      break;
    case 3:
      ListStyle = "dl";
      break;
    default:
      ListStyle = "";
  }
  BuildBulletStyleList();
}

// Save the selected index
function SelectBulleStyle()
{
  if (ListStyle == "ul")
    BulletStyleIndex = BulletStyleList.selectedIndex;
  else if (ListStyle == "ol")
    NumberStyleIndex = BulletStyleList.selectedIndex;
}

function ValidateData()
{
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    // Set the list attributes
    return true;
  }
  return false;
}
