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
 *   Baki Bon <bakibon@yahoo.com>   (original author)
 */

//------------------------------------------------------------------
// From Unicode 3.0 Page 54. 3.11 Conjoining Jamo Behavior
var SBase = 0xac00;
var LBase = 0x1100;
var VBase = 0x1161;
var TBase = 0x11A7;
var LCount = 19;
var VCount = 21;
var TCount = 28;
var NCount = VCount * TCount;
// End of Unicode 3.0

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  StartupLatin();

  doSetOKCancel(onOK, Cancel);


  // Dialog is non-modal:
  // Change "Ok" to "Insert, change "Cancel" to "Close"
  var okButton = document.getElementById("ok");
  if (okButton)
    okButton.setAttribute("label", GetString("Insert"));

  var cancelButton = document.getElementById("cancel");
  if (cancelButton)
    cancelButton.setAttribute("label", GetString("Close"));

  // Set a variable on the opener window so we
  //  can track ownership of close this window with it
  window.opener.InsertCharWindow = window;
  window.sizeToContent();

  SetWindowLocation();
}

function onOK()
{
  // Insert the character
  // Note: Assiated parent window and editorShell
  //  will be changed to whatever editor window has the focus
  window.editorShell.InsertSource(LatinChar);

  // Set persistent attributes to save
  //  which category, letter, and character modifier was used
  CategoryGroup.setAttribute("category", category);
  CategoryGroup.setAttribute("letter_index", indexL);
  CategoryGroup.setAttribute("char_index", indexM);

  // Return true only for modal window
  //return true;
}

// Don't allow inserting in HTML Source Mode
function onFocus()
{
  var enable = true;
  if ("gEditorDisplayMode" in window.opener)
    enable = !window.opener.IsInHTMLSourceMode();

  SetElementEnabledById("ok", enable);
}

function Unload()
{
  window.opener.InsertCharWindow = null;
  SaveWindowLocation();
}

function Cancel()
{
  // This will trigger call to "Unload()"
  window.close();
}

//------------------------------------------------------------------
var LatinL;
var LatinM;
var LatinL_Label;
var LatinM_Label;
var LatinChar;
var indexL=0;
var indexM=0;
var indexM_AU=0;
var indexM_AL=0;
var indexM_U=0;
var indexM_L=0;
var indexM_S=0;
var LItems=0;
var category;
var CategoryGroup;
var initialize = true;

function StartupLatin()
{

  LatinL = document.getElementById("LatinL");
  LatinM = document.getElementById("LatinM");
  LatinL_Label = document.getElementById("LatinL_Label");
  LatinM_Label = document.getElementById("LatinM_Label");

  var Symbol      = document.getElementById("Symbol");
  var AccentUpper = document.getElementById("AccentUpper");
  var AccentLower = document.getElementById("AccentUpper");
  var Upper       = document.getElementById("Upper");
  var Lower       = document.getElementById("Lower");
  CategoryGroup   = document.getElementById("CatGrp");

  // Initialize which radio button is set from persistent attribute...
  var category = CategoryGroup.getAttribute("category");

  // ...as well as indexes into the letter and character lists
  var index = Number(CategoryGroup.getAttribute("letter_index"));
  if (index && index >= 0)
    indexL = index;
  index = Number(CategoryGroup.getAttribute("char_index"));
  if (index && index >= 0)
    indexM = index;


  switch (category)
  {
    case "AccentUpper": // Uppercase Diacritical
      AccentUpper.checked = true;
      indexM_AU = indexM;
      break;
    case "AccentLower": // Lowercase Diacritical
      AccentLower.checked = true;
      indexM_AL = indexM;
      break;
    case "Upper": // Uppercase w/o Diacritical
      Upper.checked = true;
      indexM_U = indexM;
      break;
    case "Lower": // Lowercase w/o Diacritical
      Lower.checked = true;
      indexM_L = indexM;
      break;
    default:
      category = "Symbol";
      Symbol.checked = true;
      indexM_S = indexM;
      break;
  }

  ChangeCategory(category);
  initialize = false;
}

function ChangeCategory(newCategory)
{
  if (category != newCategory || initialize)
  {
    category = newCategory;
    // Note: Must do L before M to set LatinL.selectedIndex
    UpdateLatinL();
    UpdateLatinM();
    UpdateCharacter();
  }
}

function SelectLatinLetter()
{
  if(LatinL.selectedIndex != indexL )
  {
    indexL = LatinL.selectedIndex;
    UpdateLatinM();
    UpdateCharacter();
  }
}

function SelectLatinModifier()
{
  if(LatinM.selectedIndex != indexM )
  {
    indexM = LatinM.selectedIndex;
    UpdateCharacter();
  }
}
function DisableLatinL(disable)
{
  LatinL_Label.setAttribute("disabled", disable ? "true" : "false");
  LatinL.setAttribute("disabled", disable ? "true" : "false");
}

function UpdateLatinL()
{
  ClearMenulist(LatinL);
  if (category == "AccentUpper" || category == "AccentLower")
  {
    DisableLatinL(false);
    var basic;

    // Fill the list
    if (category == "AccentUpper")   // Uppercase Diacritical
    {
      for(basic=0; basic < 26; basic++)
        AppendStringToMenulist(LatinL , String.fromCharCode(0x41 + basic));
    }
    else  // Lowercase Diacritical
    {
      for(basic=0; basic < 26; basic++)
       AppendStringToMenulist(LatinL , String.fromCharCode(0x61 + basic));
    }
    // Set the selected item
    if (indexL > 25)
      indexL = 25;
    LatinL.selectedIndex = indexL;
  }
  else
  {
    // Other categories don't hinge on a "letter"
    DisableLatinL(true);
    // Note: don't change the indexL so it can be used next time
  }
}

function UpdateLatinM()
{
  ClearMenulist(LatinM);
  var i, basic;
  switch(category)
  {
    case "AccentUpper": // Uppercase Diacritical
      for(basic=0; basic < upper[indexL].length; basic++)
        AppendStringToMenulist(LatinM ,
             String.fromCharCode(upper[indexL][basic]));

      if(indexM_AU < upper[indexL].length)
        indexM = indexM_AU;
      else
        indexM = upper[indexL].length - 1;
      indexM_AU = indexM;
      break;

    case "AccentLower": // Lowercase Diacritical
      for(basic=0; basic < lower[indexL].length; basic++)
        AppendStringToMenulist(LatinM ,
             String.fromCharCode(lower[indexL][basic]));

      if(indexM_AL < lower[indexL].length)
        indexM = indexM_AL;
      else
        indexM = lower[indexL].length - 1;
      indexM_AL = indexM;
      break;

    case "Upper": // Uppercase w/o Diacritical
      for(i=0; i < otherupper.length; i++)
        AppendStringToMenulist(LatinM, String.fromCharCode(otherupper[i]));

      if(indexM_U < otherupper.length)
        indexM = indexM_U;
      else
        indexM = otherupper.length - 1;
      indexM_U = indexM;
      break;

    case "Lower": // Lowercase w/o Diacritical
      for(i=0; i < otherlower.length; i++)
        AppendStringToMenulist(LatinM , String.fromCharCode(otherlower[i]));

      if(indexM_L < otherlower.length)
        indexM = indexM_L;
      else
        indexM = otherlower.length - 1;
      indexM_L = indexM;
      break;

    case "Symbol": // Symbol
      for(i=0; i < symbol.length; i++)
        AppendStringToMenulist(LatinM , String.fromCharCode(symbol[i]));

      if(indexM_S < symbol.length)
        indexM = indexM_S;
      else
        indexM = symbol.length - 1;
      indexM_S = indexM;
      break;
  }
  LatinM.selectedIndex = indexM;
}

function UpdateCharacter()
{
  indexM = LatinM.selectedIndex;

  switch(category)
  {
    case "AccentUpper": // Uppercase Diacritical
      LatinChar = String.fromCharCode(upper[indexL][indexM]);
      indexM_AU = indexM;
      break;
    case "AccentLower": // Lowercase Diacritical
      LatinChar = String.fromCharCode(lower[indexL][indexM]);
      indexM_AL = indexM;
      break;
    case "Upper": // Uppercase w/o Diacritical
      LatinChar = String.fromCharCode(otherupper[indexM]);
      indexM_U = indexM;
      break;
    case "Lower": // Lowercase w/o Diacritical
      LatinChar = String.fromCharCode(otherlower[indexM]);
      indexM_L = indexM;
      break;
    case "Symbol":
      LatinChar =  String.fromCharCode(symbol[indexM]);
      indexM_S = indexM;
      break;
  }
//dump("Letter Index="+indexL+", Character Index="+indexM+", Character = "+LatinChar+"\n");
}

var upper=[
[ // A
  0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5,
  0x0100, 0x0102, 0x0104, 0x01cd, 0x01de, 0x01de, 0x01e0, 0x01fa,
  0x0200, 0x0202, 0x0226,
  0x1e00, 0x1ea0, 0x1ea2, 0x1ea4, 0x1ea6, 0x1ea8, 0x1eaa, 0x1eac,
  0x1eae, 0x1eb0, 0x1eb2, 0x1eb4, 0x1eb6
], [ // B
  0x0181, 0x0182, 0x0184,
  0x1e02, 0x1e04, 0x1e06
], [ // C
  0x00c7, 0x0106, 0x0108, 0x010a, 0x010c,
  0x0187,
  0x1e08
], [ // D
  0x010e, 0x0110,
  0x0189,
  0x018a,
  0x1e0a, 0x1e0c, 0x1e0e, 0x1e10, 0x1e12
], [ // E
  0x00C8, 0x00C9, 0x00CA, 0x00CB,
  0x0112, 0x0114, 0x0116, 0x0118, 0x011A,
  0x0204, 0x0206, 0x0228,
  0x1e14, 0x1e16, 0x1e18, 0x1e1a, 0x1e1c,
  0x1eb8, 0x1eba, 0x1ebc, 0x1ebe, 0x1ec0, 0x1ec2, 0x1ec4, 0x1ec6
], [ // F
  0x1e1e
], [ // G
  0x011c, 0x011E, 0x0120, 0x0122,
  0x01e4, 0x01e6, 0x01f4,
  0x1e20
], [ // H
  0x0124, 0x0126,
  0x021e,
  0x1e22, 0x1e24, 0x1e26, 0x1e28, 0x1e2a
], [ // I
  0x00CC, 0x00CD, 0x00CE, 0x00CF,
  0x0128, 0x012a, 0x012C, 0x012e, 0x0130,
  0x0208, 0x020a,
  0x1e2c, 0x1e2e,
  0x1ec8, 0x1eca
], [ // J
  0x0134,
  0x01f0
], [ // K
  0x0136,
  0x0198, 0x01e8,
  0x1e30, 0x1e32, 0x1e34
], [ // L
  0x0139, 0x013B, 0x013D, 0x013F, 0x0141,
  0x1e36, 0x1e38, 0x1e3a, 0x1e3c
], [ // M
  0x1e3e, 0x1e40, 0x1e42
], [ // N
  0x00D1,
  0x0143, 0x0145, 0x0147, 0x014A,
  0x01F8,
  0x1e44, 0x1e46, 0x1e48, 0x1e4a
], [ // O
  0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6,
  0x014C, 0x014E, 0x0150,
  0x01ea, 0x01ec,
  0x020c, 0x020e, 0x022A, 0x022C, 0x022E, 0x0230,
  0x1e4c, 0x1e4e, 0x1e50, 0x1e52,
  0x1ecc, 0x1ece, 0x1ed0, 0x1ed2, 0x1ed4, 0x1ed6, 0x1ed8, 0x1eda, 0x1edc, 0x1ede,
  0x1ee0, 0x1ee2
], [ // P
  0x1e54, 0x1e56
], [ // Q
  0x0051
], [ // R
  0x0154, 0x0156, 0x0158,
  0x0210, 0x0212,
  0x1e58, 0x1e5a, 0x1e5c, 0x1e5e
], [ // S
  0x015A, 0x015C, 0x015E, 0x0160,
  0x0218,
  0x1e60, 0x1e62, 0x1e64, 0x1e66, 0x1e68
], [ // T
  0x0162, 0x0164, 0x0166,
  0x021A,
  0x1e6a, 0x1e6c, 0x1e6e, 0x1e70
], [ // U
  0x00D9, 0x00DA, 0x00DB, 0x00DC,
  0x0168, 0x016A, 0x016C, 0x016E, 0x0170, 0x0172,
  0x0214, 0x0216,
  0x1e72, 0x1e74, 0x1e76, 0x1e78, 0x1e7a,
  0x1ee4, 0x1ee6, 0x1ee8, 0x1eea, 0x1eec, 0x1eee, 0x1ef0
], [ // V
  0x1e7c, 0x1e7e
], [ // W
  0x0174,
  0x1e80, 0x1e82, 0x1e84, 0x1e86, 0x1e88
], [ // X
  0x1e8a, 0x1e8c
], [ // Y
  0x00DD,
  0x0176, 0x0178,
  0x0232,
  0x1e8e,
  0x1ef2, 0x1ef4, 0x1ef6, 0x1ef8
], [ // Z
  0x0179, 0x017B, 0x017D,
  0x0224,
  0x1e90, 0x1e92, 0x1e94
] ];
var lower=[
[ // a
  0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5,
  0x0101, 0x0103, 0x0105,
  0x01ce, 0x01df, 0x01e1, 0x01fb,
  0x0201, 0x0203, 0x0227,
  0x1e01, 0x1e9a,
  0x1ea1, 0x1ea3, 0x1ea5, 0x1ea7, 0x1ea9, 0x1eab, 0x1ead, 0x1eaf,
  0x1eb1, 0x1eb3, 0x1eb5, 0x1eb7
], [ // b
  0x0180, 0x0183, 0x0185,
  0x1e03, 0x1e05, 0x1e07
], [ // c
  0x00e7,
  0x0107, 0x0109, 0x010b, 0x010d,
  0x0188,
  0x1e09
], [ // d
  0x010f, 0x0111,
  0x1e0b, 0x1e0d, 0x1e0f, 0x1e11, 0x1e13
], [ // e
  0x00e8, 0x00e9, 0x00ea, 0x00eb,
  0x0113, 0x0115, 0x0117, 0x0119, 0x011b,
  0x0205, 0x0207, 0x0229,
  0x1e15, 0x1e17, 0x1e19, 0x1e1b, 0x1e1d,
  0x1eb9, 0x1ebb, 0x1ebd, 0x1ebf,
  0x1ec1, 0x1ec3, 0x1ec5, 0x1ec7
], [ // f
  0x1e1f
], [ // g
  0x011d, 0x011f, 0x0121, 0x0123,
  0x01e5, 0x01e7, 0x01f5,
  0x1e21
], [ // h
  0x0125, 0x0127,
  0x021f,
  0x1e23, 0x1e25, 0x1e27, 0x1e29, 0x1e2b, 0x1e96
], [ // i
  0x00ec, 0x00ed, 0x00ee, 0x00ef,
  0x0129, 0x012b, 0x012d, 0x012f, 0x0131,
  0x01d0,
  0x0209, 0x020b,
  0x1e2d, 0x1e2f,
  0x1ec9, 0x1ecb
], [ // j
  0x0135,
], [ // k
  0x0137, 0x0138,
  0x01e9,
  0x1e31, 0x1e33, 0x1e35
], [ // l
  0x013a, 0x013c, 0x013e, 0x0140, 0x0142,
  0x1e37, 0x1e39, 0x1e3b, 0x1e3d
], [ // m
  0x1e3f, 0x1e41, 0x1e43
], [ // n
  0x00f1,
  0x0144, 0x0146, 0x0148, 0x0149, 0x014b,
  0x01f9,
  0x1e45, 0x1e47, 0x1e49, 0x1e4b
], [ // o
  0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6,
  0x014d, 0x014f, 0x0151,
  0x01d2, 0x01eb, 0x01ed,
  0x020d, 0x020e, 0x022b, 0x22d, 0x022f, 0x0231,
  0x1e4d, 0x1e4f, 0x1e51, 0x1e53,
  0x1ecd, 0x1ecf,
  0x1ed1, 0x1ed3, 0x1ed5, 0x1ed7, 0x1ed9, 0x1edb, 0x1edd, 0x1edf,
  0x1ee1, 0x1ee3
], [ // p
  0x1e55, 0x1e57
], [ // q
  0x0071
], [ // r
  0x0155, 0x0157, 0x0159,
  0x0211, 0x0213,
  0x1e59, 0x1e5b, 0x1e5d, 0x1e5f
], [ // s
  0x015b, 0x015d, 0x015f, 0x0161,
  0x0219,
  0x1e61, 0x1e63, 0x1e65, 0x1e67, 0x1e69
], [ // t
  0x0162, 0x0163, 0x0165, 0x0167,
  0x021b,
  0x1e6b, 0x1e6d, 0x1e6f, 0x1e71,
  0x1e97
], [ // u
  0x00f9, 0x00fa, 0x00fb, 0x00fc,
  0x0169, 0x016b, 0x016d, 0x016f, 0x0171, 0x0173,
  0x01d4, 0x01d6, 0x01d8, 0x01da, 0x01dc,
  0x0215, 0x0217,
  0x1e73, 0x1e75, 0x1e77, 0x1e79, 0x1e7b,
  0x1ee5, 0x1ee7, 0x1ee9, 0x1eeb, 0x1eed, 0x1eef,
  0x1ef1
], [ // v
  0x1e7d, 0x1e7f
], [ // w
  0x0175,
  0x1e81, 0x1e83, 0x1e85, 0x1e87, 0x1e89, 0x1e98,
], [ // x
  0x1e8b, 0x1e8d
], [ // y
  0x00fd, 0x00ff,
  0x0177,
  0x0233,
  0x1e8f, 0x1e99, 0x1ef3, 0x1ef5, 0x1ef7, 0x1ef9
], [ // z
  0x017a, 0x017c, 0x017e,
  0x0225,
  0x1e91, 0x1e93, 0x1e95
] ];


var symbol = [
        0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac,         0x00ae, 0x00af,
0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
0x00d7, 0x00f7
];

var otherupper = [
0x00c6, 0x00d0, 0x00d8, 0x00de,
0x0132,
0x0152,
0x0186,
0x01c4, 0x01c5,
0x01c7, 0x01c8,
0x01ca, 0x01cb,
0x01F1, 0x01f2
];
var otherlower = [
0x00e6, 0x00f0, 0x00f8, 0x00fe, 0x00df,
0x0133,
0x0153,
0x01c6,
0x01c9,
0x01cc,
0x01f3
];
