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
 *   Neil Rashbrook <neil@parkwaycc.co.uk>
 */

//Cancel() is in EdDialogCommon.js
var gBulletStyleType = "";
var gNumberStyleType = "";
var gListElement;
var gOriginalListType = "";
var gListType = "";
var gMixedListSelection = false;
var gStyleType = "";
var gOriginalStyleType = "";
const gOnesArray = ["", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"];
const gTensArray = ["", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC"];
const gHundredsArray = ["", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM"];
const gThousandsArray = ["", "M", "MM", "MMM", "MMMM", "MMMMM", "MMMMMM", "MMMMMMM", "MMMMMMMM", "MMMMMMMMM"];
const gRomanDigits = {I: 1, V: 5, X: 10, L: 50, C: 100, D: 500, M: 1000};
const A = "A".charCodeAt(0);
const gArabic = "1";
const gUpperRoman = "I";
const gLowerRoman = "i";
const gUpperLetters = "A";
const gLowerLetters = "a";
const gDecimalCSS = "decimal";
const gUpperRomanCSS = "upper-roman";
const gLowerRomanCSS = "lower-roman";
const gUpperAlphaCSS = "upper-alpha";
const gLowerAlphaCSS = "lower-alpha";

// dialog initialization code
function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }
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
  try {
    gListType = editor.getListState(mixedObj, {}, {}, {} );

    // We may have mixed list and non-list, or > 1 list type in selection
    gMixedListSelection = mixedObj.value;

    // Get the list element at the anchor node
    gListElement = editor.getElementOrParentByTagName("list", null);
  } catch (e) {}

  // The copy to use in AdvancedEdit
  if (gListElement)
    globalElement = gListElement.cloneNode(false);

  // Show extra options for changing entire list if we have one already.
  gDialog.RadioGroup.collapsed = !gListElement;
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
  
  gDialog.ListTypeList.value = gListType;
  gDialog.StartingNumberInput.value = "";
  
  // Last param = true means attribute value is case-sensitive
  var type = globalElement ? GetHTMLOrCSSStyleValue(globalElement, "type", "list-style-type") : null;

  var index = 0;
  if (gListType == "ul")
  {
    if (type)
    {
      type = type.toLowerCase();
      gBulletStyleType = type;
      gOriginalStyleType = type;
    }
  }
  else if (gListType == "ol")
  {
    // Translate CSS property strings
    switch (type.toLowerCase())
    {
      case gDecimalCSS:
        type = gArabic;
        break;
      case gUpperRomanCSS:
        type = gUpperRoman;
        break;
      case gLowerRomanCSS:
        type = gLowerRoman;
        break;
      case gUpperAlphaCSS:
        type = gUpperLetters;
        break;
      case gLowerAlphaCSS:
        type = gLowerLetters;
        break;
    }
    if (type)
    {
      gNumberStyleType = type;
      gOriginalStyleType = type;
    }

    // Convert attribute number to appropriate letter or roman numeral
    gDialog.StartingNumberInput.value = 
      ConvertStartAttrToUserString(globalElement.getAttribute("start"), type);
  }
  BuildBulletStyleList();
}

// Convert attribute number to appropriate letter or roman numeral
function ConvertStartAttrToUserString(startAttr, type)
{
  switch (type)
  {
    case gUpperRoman:
      startAttr = ConvertArabicToRoman(startAttr);
      break;
    case gLowerRoman:
      startAttr = ConvertArabicToRoman(startAttr).toLowerCase();
      break;
    case gUpperLetters:
      startAttr = ConvertArabicToLetters(startAttr);
      break;
    case gLowerLetters:
      startAttr = ConvertArabicToLetters(startAttr).toLowerCase();
      break;
  }
  return startAttr;
}

function BuildBulletStyleList()
{
  gDialog.BulletStyleList.removeAllItems();
  var label;

  if (gListType == "ul")
  {
    gDialog.BulletStyleList.removeAttribute("disabled");
    gDialog.BulletStyleLabel.removeAttribute("disabled");
    gDialog.StartingNumberInput.setAttribute("disabled", "true");
    gDialog.StartingNumberLabel.setAttribute("disabled", "true");

    label = GetString("BulletStyle");

    gDialog.BulletStyleList.appendItem(GetString("Automatic"), "");
    gDialog.BulletStyleList.appendItem(GetString("SolidCircle"), "disc");
    gDialog.BulletStyleList.appendItem(GetString("OpenCircle"), "circle");
    gDialog.BulletStyleList.appendItem(GetString("SolidSquare"), "square");

    gDialog.BulletStyleList.value = gBulletStyleType;
  }
  else if (gListType == "ol")
  {
    gDialog.BulletStyleList.removeAttribute("disabled");
    gDialog.BulletStyleLabel.removeAttribute("disabled");
    gDialog.StartingNumberInput.removeAttribute("disabled");
    gDialog.StartingNumberLabel.removeAttribute("disabled");
    label = GetString("NumberStyle");

    gDialog.BulletStyleList.appendItem(GetString("Automatic"), "");
    gDialog.BulletStyleList.appendItem(GetString("Style_1"), gArabic);
    gDialog.BulletStyleList.appendItem(GetString("Style_I"), gUpperRoman);
    gDialog.BulletStyleList.appendItem(GetString("Style_i"), gLowerRoman);
    gDialog.BulletStyleList.appendItem(GetString("Style_A"), gUpperLetters);
    gDialog.BulletStyleList.appendItem(GetString("Style_a"), gLowerLetters);

    gDialog.BulletStyleList.value = gNumberStyleType;
  } 
  else 
  {
    gDialog.BulletStyleList.setAttribute("disabled", "true");
    gDialog.BulletStyleLabel.setAttribute("disabled", "true");
    gDialog.StartingNumberInput.setAttribute("disabled", "true");
    gDialog.StartingNumberLabel.setAttribute("disabled", "true");
  }
  
  // Disable advanced edit button if changing to "normal"
  if (gListType)
    gDialog.AdvancedEditButton.removeAttribute("disabled");
  else
    gDialog.AdvancedEditButton.setAttribute("disabled", "true");

  if (label)
    gDialog.BulletStyleLabel.setAttribute("label",label);
}

function SelectListType()
{
  // Each list type is stored in the "value" of each menuitem
  var NewType = gDialog.ListTypeList.value;

  if (NewType == "ol")
    SetTextboxFocus(gDialog.StartingNumberInput);

  if (gListType != NewType)
  {
    gListType = NewType;
    
    // Create a newlist object for Advanced Editing
    try {
      if (gListType)
        globalElement = GetCurrentEditor().createElementWithDefaults(gListType);
    } catch (e) {}

    BuildBulletStyleList();
  }
}

function SelectBulletStyle()
{
  // Save the selected index so when user changes
  //   list style, restore index to associated list
  // Each bullet or number type is stored in the "value" of each menuitem
  if (gListType == "ul")
    gBulletStyleType = gDialog.BulletStyleList.value;
  else if (gListType == "ol")
  {
    var type = gDialog.BulletStyleList.value;
    if (gNumberStyleType != type)
    {
      // Convert existing input value to attr number first,
      //   then convert to the appropriate format for the newly-selected
      gDialog.StartingNumberInput.value = 
        ConvertStartAttrToUserString( ConvertUserStringToStartAttr(gNumberStyleType), type);

      gNumberStyleType = type;
      SetTextboxFocus(gDialog.StartingNumberInput);
    }
  }
}

function ValidateData()
{
  gBulletStyleType = gDialog.BulletStyleList.value;
  // globalElement should already be of the correct type 

  if (globalElement)
  {
    var editor = GetCurrentEditor();
    if (gListType == "ul")
    {
      if (gBulletStyleType && gDialog.ChangeAllRadio.selected)
        globalElement.setAttribute("type", gBulletStyleType);
      else
        try {
          editor.removeAttributeOrEquivalent(globalElement, "type", true);
        } catch (e) {}

    } 
    else if (gListType == "ol")
    {
      if (gBulletStyleType)
        globalElement.setAttribute("type", gBulletStyleType);
      else
        try {
          editor.removeAttributeOrEquivalent(globalElement, "type", true);
        } catch (e) {}
      
      var startingNumber = ConvertUserStringToStartAttr(gBulletStyleType);
      if (startingNumber)
        globalElement.setAttribute("start", startingNumber);
      else
        globalElement.removeAttribute("start");
    }
  }
  return true;
}

function ConvertUserStringToStartAttr(type)
{
  var startingNumber = TrimString(gDialog.StartingNumberInput.value);

  switch (type)
  {
    case gUpperRoman:
    case gLowerRoman:
      // If the input isn't an integer, assume it's a roman numeral. Convert it.
      if (!Number(startingNumber))
        startingNumber = ConvertRomanToArabic(startingNumber);
      break;
    case gUpperLetters:
    case gLowerLetters:
      // Get the number equivalent of the letters
      if (!Number(startingNumber))
        startingNumber = ConvertLettersToArabic(startingNumber);
      break;
  }
  return startingNumber;
}

function ConvertRomanToArabic(num)
{
  num = num.toUpperCase();
  if (num && !/[^MDCLXVI]/i.test(num))
  {
    var Arabic = 0;
    var last_digit = 1000;
    for (var i=0; i < num.length; i++)
    {
      var digit = gRomanDigits[num.charAt(i)];
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

function onAccept()
{
  if (ValidateData())
  {
    // Coalesce into one undo transaction
    var editor = GetCurrentEditor();

    editor.beginTransaction();

    var changeEntireList = gDialog.RadioGroup.selectedItem == gDialog.ChangeAllRadio;

    // Remember which radio button was selected
    if (gListElement)
      gDialog.RadioGroup.setAttribute("index", changeEntireList ? "0" : "1");

    var changeList;
    if (gListElement && gDialog.ChangeAllRadio.selected)
    {
      changeList = true;
    }
    else
      changeList = gMixedListSelection || gListType != gOriginalListType || 
                   gBulletStyleType != gOriginalStyleType;
    if (changeList)
    {
      try {
        if (gListType)
        {
          editor.makeOrChangeList(gListType, changeEntireList,
                     (gBulletStyleType != gOriginalStyleType) ? gBulletStyleType : null);

          // Get the new list created:
          gListElement = editor.getElementOrParentByTagName(gListType, null);

          editor.cloneAttributes(gListElement, globalElement);
        }
        else
        {
          // Remove all existing lists
          if (gListElement && changeEntireList)
            editor.selectElement(gListElement);

          editor.removeList("ol");
          editor.removeList("ul");
          editor.removeList("dl");
        }
      } catch (e) {}
    }

    editor.endTransaction();
    
    SaveWindowLocation();

    return true;
  }
  return false;
}
