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
const gOnesArray = ["", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"];
const gTensArray = ["", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC"];
const gHundredsArray = ["", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM"];
const gThousandsArray = ["", "M", "MM", "MMM", "MMMM", "MMMMM", "MMMMMM", "MMMMMMM", "MMMMMMMM", "MMMMMMMMM"];
const A = "A".charCodeAt(0);

// These indexes must correspond to order of items in menulists:
const UL_LIST = 1;
const OL_LIST = 2;
const DL_LIST = 3;
const UL_TYPE_DISC = 1;
const UL_TYPE_CIRCLE = 2;
const UL_TYPE_SQUARE = 3;
const OL_TYPE_DECIMAL = 1;
const OL_TYPE_UPPER_ROMAN = 2;
const OL_TYPE_LOWER_ROMAN = 3;
const OL_TYPE_UPPER_ALPHA = 4;
const OL_TYPE_LOWER_ALPHA = 5;

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
  var mixedObj = { value: null };
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
        index = UL_TYPE_DISC;
      else if (type == "circle")
        index = UL_TYPE_CIRCLE;
      else if (type == "square")
        index = UL_TYPE_SQUARE;
    }
  }
  else if (gListType == "ol")
  {
    switch (type)
    {
      case "1":
      case "decimal":
        index = OL_TYPE_DECIMAL;
        break;
      case "I":
      case "upper-roman":
        index = OL_TYPE_UPPER_ROMAN;
        break;
      case "i":
      case "lower-roman":
        index = OL_TYPE_LOWER_ROMAN;
        break;
      case "A":
      case "upper-alpha":
        index = OL_TYPE_UPPER_ALPHA;
        break;
      case "a":
      case "lower-alpha":
        index = OL_TYPE_LOWER_ALPHA;
        break;
    }
    gNumberStyleIndex = index;
  }
  gDialog.BulletStyleList.selectedIndex = index;

  if (globalElement)
    // Convert attribute number to appropriate letter or roman numeral
    gDialog.StartingNumberInput.value = 
      ConvertStartAttrToUserString(globalElement.getAttribute("start"), index);

  gOriginalBulletStyleType = type;
}

// Convert attribute number to appropriate letter or roman numeral
function ConvertStartAttrToUserString(startAttr, numberStyleIndex)
{
  switch (numberStyleIndex)
  {
    case OL_TYPE_UPPER_ROMAN:
      startAttr = ConvertArabicToRoman(startAttr);
      break;
    case OL_TYPE_LOWER_ROMAN:
      startAttr = ConvertArabicToRoman(startAttr).toLowerCase();
      break;
    case OL_TYPE_UPPER_ALPHA:
      startAttr = ConvertArabicToLetters(startAttr);
      break;
    case OL_TYPE_LOWER_ALPHA:
      startAttr = ConvertArabicToLetters(startAttr).toLowerCase();
      break;
  }
  return startAttr;
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
    gDialog.ListTypeList.selectedIndex = UL_LIST;
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
    gDialog.ListTypeList.selectedIndex = OL_LIST;
  } 
  else 
  {
    gDialog.BulletStyleList.setAttribute("disabled", "true");
    gDialog.BulletStyleLabel.setAttribute("disabled", "true");
    gDialog.StartingNumberInput.setAttribute("disabled", "true");
    gDialog.StartingNumberLabel.setAttribute("disabled", "true");

    if (gListType == "dl")
      gDialog.ListTypeList.selectedIndex = DL_LIST;
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
    case UL_LIST:
      NewType = "ul";
      break;
    case OL_LIST:
      NewType = "ol";
      SetTextboxFocus(gDialog.StartingNumberInput);
      break;
    case DL_LIST:
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
        case UL_TYPE_DISC:
          type = "disc";
          break;
        case UL_TYPE_CIRCLE:
          type = "circle";
          break;
        case UL_TYPE_SQUARE:
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
        case OL_TYPE_DECIMAL:
          type = "1";
          break;
        case OL_TYPE_UPPER_ROMAN:
          type = "I";
          break;
        case OL_TYPE_LOWER_ROMAN:
          type = "i";
          break;
        case OL_TYPE_UPPER_ALPHA:
          type = "A";
          break;
        case OL_TYPE_LOWER_ALPHA:
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
    case OL_TYPE_UPPER_ROMAN:
    case OL_TYPE_LOWER_ROMAN:
      // If the input isn't an integer, assume it's a roman numeral. Convert it.
      if (!Number(startingNumber))
        startingNumber = ConvertRomanToArabic(startingNumber);
      break;
    case OL_TYPE_UPPER_ALPHA:
    case OL_TYPE_LOWER_ALPHA:
      // Get the number equivalent of the letters
      if (!Number(startingNumber))
        startingNumber = ConvertLettersToArabic(startingNumber);
      break;
  }
  return startingNumber;
}

function ConvertLettersToArabic(letters)
{
  letters = letters.toUpperCase();
  if (!letters || /[^A-Z]/.test(letters))
    return "";

  var num = 0;
  for (var i = 0; i < letters.length; i++)
    num = num * 26 + letters.charCodeAt(i) - A + 1;
  return num;
}

function ConvertArabicToLetters(num)
{
  var letters = "";
  while (num) {
    num--;
    letters = String.fromCharCode(A + (num % 26)) + letters;
    num = Math.floor(num / 26);
  }
  return letters;
}

function ConvertRomanToArabic(num)
{
  num = num.toUpperCase();
  if (num && !/[^MDCLXVI]/i.test(num))
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

function ConvertArabicToRoman(num)
{
  if (/^\d{1,4}$/.test(num))
  {
    var digits = ("000" + num).substr(-4);
    return gThousandsArray[digits.charAt(0)] +
           gHundredsArray[digits.charAt(1)] +
           gTensArray[digits.charAt(2)] +
           gOnesArray[digits.charAt(3)];
  }
  return "";
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
