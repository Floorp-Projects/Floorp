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
 *   Charles Manske (cmanske@netscape.com)
 *   Ryan Cassin (rcassin@supernova.org)
 *   David Turley (dturley@pobox.com) contributed Roman Numeral conversion code.
 */

//Cancel() is in EdDialogCommon.js
var gBulletStyleIndex = 0;
var gNumberStyleIndex = 0;
var gListElement;
var gOriginalListType = "";
var gListType = "";
var gMixedListSelection = false;
var gBulletStyleType = "";
var gOriginalBulletStyleType = "";
var gOnesArray = ["I","II","III","IV","V","VI","VII","VIII","IX"];
var gTensArray = ["X","XX","XXX","XL","L","LX","LXX","LXXX","XC"];
var gHundredsArray = ["C","CC","CCC","CD","D","DC","DCC","DCCC","CM"];

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

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
  gListType = editorShell.GetListState(mixedObj);
  // We may have mixed list and non-list, or > 1 list type in selection
  gMixedListSelection = mixedObj.value;

  // Get the list element at the anchor node
  gListElement = editorShell.GetElementOrParentByTagName("list", null);

  // The copy to use in AdvancedEdit
  if (gListElement)
    globalElement = gListElement.cloneNode(false);

  // Show extra options for changing entire list if we have one already.
  gDialog.RadioGroup.setAttribute("collapsed", gListElement ? "false" : "true");
  if (gListElement)
  {
    // Radio button index is persistant
    if (gDialog.RadioGroup.getAttribute("index") == "1")
      gDialog.RadioGroup.selectedItem = gDialog.ChangeSelectedRadio;
    else
      gDialog.RadioGroup.selectedItem = gDialog.ChangeAllRadio;
  }

  InitDialog();

  gOriginalListType = gListType;

  gDialog.ListTypeList.focus();

  SetWindowLocation();
}

function InitDialog()
{
  // Note that if mixed, we we pay attention 
  //   only to the anchor node's list type
  // (i.e., don't confuse user with "mixed" designation)
  if (gListElement)
    gListType = gListElement.nodeName.toLowerCase();
  else
    gListType = "";
  
  BuildBulletStyleList();
  gDialog.StartingNumberInput.value = "";
  
  // Last param = true means attribute value is case-sensitive
  var type = globalElement ? GetHTMLOrCSSStyleValue(globalElement, "type", "list-style-type") : null;

  var index = 0;
  if (gListType == "ul")
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
  else if (gListType == "ol")
  {
    switch (type)
    {
      case "1":
      case "decimal":
        index = 1;
        break;
      case "I":
      case "upper-roman":
        index = 2;
        break;
      case "i":
      case "lower-roman":
        index = 3;
        break;
      case "A":
      case "upper-alpha":
        index = 4;
        break;
      case "a":
      case "lower-alpha":
        index = 5;
        break;
    }
    gNumberStyleIndex = index;
  }
  gDialog.BulletStyleList.selectedIndex = index;

  // Convert attribute number to appropriate letter or roman numeral
  gDialog.StartingNumberInput.value = 
    ConvertStartAttrToUserString(globalElement.getAttribute("start"), index);

  gOriginalBulletStyleType = type;
}

// Convert attribute number to appropriate letter or roman numeral
function ConvertStartAttrToUserString(startAttr, numberStyleIndex)
{
  if (!startAttr)
    return startAttr;

  var start = "";
  switch (numberStyleIndex)
  {
    case 1:
      start = startAttr;
      break;
    case 2:
      start = toRoman(startAttr);
      break;
    case 3:
      start = toRoman(startAttr).toLowerCase();
      break;
    case 4:
      start = String.fromCharCode(Number(startAttr) + 64);
      break;
    case 5:
      start = String.fromCharCode(Number(startAttr) + 96);
      break;
  }
  return start;
}

function BuildBulletStyleList()
{
  ClearMenulist(gDialog.BulletStyleList);
  var label;

  if (gListType == "ul")
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

    gDialog.BulletStyleList.selectedIndex = gBulletStyleIndex;
    gDialog.ListTypeList.selectedIndex = 1;
  }
  else if (gListType == "ol")
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

    gDialog.BulletStyleList.selectedIndex = gNumberStyleIndex;
    gDialog.ListTypeList.selectedIndex = 2;
  } 
  else 
  {
    gDialog.BulletStyleList.setAttribute("disabled", "true");
    gDialog.BulletStyleLabel.setAttribute("disabled", "true");
    gDialog.StartingNumberInput.setAttribute("disabled", "true");
    gDialog.StartingNumberLabel.setAttribute("disabled", "true");

    if (gListType == "dl")
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
  if (gListType)
    gDialog.AdvancedEditButton.removeAttribute("disabled");
  else
    gDialog.AdvancedEditButton.setAttribute("disabled","true");

  if (label)
    gDialog.BulletStyleLabel.setAttribute("label",label);
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
  if (gListType != NewType)
  {
    gListType = NewType;
    
    // Create a newlist object for Advanced Editing
    if (gListType)
      globalElement = editorShell.CreateElementWithDefaults(gListType);

    BuildBulletStyleList();
  }
}

function SelectBulletStyle()
{
  // Save the selected index so when user changes
  //   list style, restore index to associated list
  if (gListType == "ul")
    gBulletStyleIndex = gDialog.BulletStyleList.selectedIndex;
  else if (gListType == "ol")
  {
    var index = gDialog.BulletStyleList.selectedIndex;
    if (gNumberStyleIndex != index)
    {
      // Convert existing input value to attr number first,
      //   then convert to the appropriate format for the newly-selected
      gDialog.StartingNumberInput.value = 
        ConvertStartAttrToUserString( ConvertUserStringToStartAttr(gNumberStyleIndex), index);

      gNumberStyleIndex = index;
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
    if (gListType == "ul")
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
      gBulletStyleType = type;
      if (type && gDialog.ChangeAllRadio.selected)
        globalElement.setAttribute("type",type);
      else
        globalElement.removeAttribute("type");

    } 
    else if (gListType == "ol")
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
      gBulletStyleType = type;
      if (type)
        globalElement.setAttribute("type", type);
      else
        globalElement.removeAttribute("type");
      
      var startingNumber = ConvertUserStringToStartAttr(gDialog.BulletStyleList.selectedIndex);
      if (startingNumber)
        globalElement.setAttribute("start",startingNumber);
      else
        globalElement.removeAttribute("start");
    }
  }
  return true;
}

function ConvertUserStringToStartAttr(selectedIndex) 
{
  var startingNumber = TrimString(gDialog.StartingNumberInput.value);

  switch (selectedIndex)
  {
    // Index 0 = automatic, the default, so we don't set it explicitly
    case 1:
      startingNumber = Number(startingNumber);
      break;
    case 2:
      // If the input isn't an integer, assume it's a roman numeral. Convert it.
      if (!Number(startingNumber))
        startingNumber = toArabic(startingNumber);
      break;
    case 3:
      // If the input isn't an integer, assume it's a roman numeral. Convert it.
      if (!Number(startingNumber))
        startingNumber = toArabic(startingNumber);
      break;
    case 4:
      // Convert to ASCII and get the number equivalent of the letter
      if (!Number(startingNumber) && startingNumber)
        startingNumber = startingNumber.toUpperCase().charCodeAt(0) - 64;
      break;
    case 5:
      // Convert to ASCII and get the number equivalent of the letter
      if (!Number(startingNumber) && startingNumber)
        startingNumber = startingNumber.toLowerCase().charCodeAt(0) - 96;
      break;
  }
  return startingNumber;
}

function toArabic(num)
{
  num = num.toUpperCase();
  if (checkRomanInput(num))
  {
    var Arabic = 0;
    var last_digit = 1000;
    var digit;
    for (var i=0; i < num.length; i++)
    {
      switch (num.charAt(i))
      {
        case  "I":
          digit=1;
          break;
        case  "V":
          digit=5;
          break;
        case  "X":
          digit=10;
          break;
        case  "L":
          digit=50;
          break;
        case  "C":
          digit=100;
          break;
        case  "D":
          digit=500;
          break;
        case  "M":
          digit=1000;
          break;
      }
      if (last_digit < digit)
        Arabic -= 2 * last_digit;

      last_digit = digit;
      Arabic += last_digit;
    }
    
    return Arabic;
  }

  return "";
}

function toRoman(num)
{
  if (checkArabicInput(num))
  {
    var ones = num % 10;
    num = (num - ones) / 10;
    var tens = num % 10;
    num = (num - tens) / 10;
    var hundreds = num % 10;
    num = (num - hundreds) / 10;

    var Roman = "";

    for (var i=0; i < num; i++)
      Roman += "M";

    if (hundreds)
      Roman += gHundredsArray[hundreds-1];

    if (tens)
      Roman += gTensArray[tens-1];

    if (ones)
      Roman += gOnesArray[ones-1];

    return Roman;
  }
  return "";
}

function checkArabicInput(num)
{
  if (!num)
    return false;

  num = String(num);
  for (var k = 0; k < num.length; k++)
  {
	  if (num.charAt(k) < "0" || num.charAt(k) > "9")
		  return false;
  }

	if (num > 4000 || num <= 0)
    return false;

	return true;
}

function checkRomanInput(num)
{
  if (!num)
    return false;

  num = num.toUpperCase();
  for (var k = 0; k < num.length; k++)
  {
    if (num.charAt(k) != "I" && num.charAt(k) != "V" && 
        num.charAt(k) != "X" && num.charAt(k) != "L" && 
        num.charAt(k) != "C" && num.charAt(k) != "D" && 
        num.charAt(k) != "M")
    {
      return false;
    }
  }
  return true;
}

function onAccept()
{
  if (ValidateData())
  {
    // Coalesce into one undo transaction
    editorShell.BeginBatchChanges();


    // Remember which radio button was selected
    if (gListElement)
      gDialog.RadioGroup.setAttribute("index", gDialog.ChangeAllRadio.selected ? "0" : "1");

    var changeList;
    if (gListElement && gDialog.ChangeAllRadio.selected)
    {
      changeList = true;
    }
    else
      changeList = gMixedListSelection || gListType != gOriginalListType || 
                   gBulletStyleType != gOriginalBulletStyleType;

    if (changeList)
    {
      editorShell.MakeOrChangeList(gListType, gDialog.ChangeAllRadio.selected,
                   (gBulletStyleType != gOriginalBulletStyleType) ? gBulletStyleType : null);

      if (gListType)
      {
        // Get the new list created:
        gListElement = editorShell.GetElementOrParentByTagName(gListType, null);
      }
      else
      {
        // We removed an existing list
        gListElement = null;
      }
    }

    // Set the new list attributes
    if (gListElement)
      editorShell.CloneAttributes(gListElement, globalElement);

    editorShell.EndBatchChanges();
    
    SaveWindowLocation();

    return true;
  }
  return false;
}
