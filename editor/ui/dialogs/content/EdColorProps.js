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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 Behavior notes:
 Radio buttons select "UseDefaultColors" vs. "UseCustomColors" modes.
 If any color attribute is set in the body, mode is "Custom Colors",
  even if 1 or more (but not all) are actually null (= "use default")
 When in "Custom Colors" mode, all colors will be set on body tag,
  even if they are just default colors, to assure compatable colors in page.
 User cannot select "use default" for individual colors
*/

//Cancel() is in EdDialogCommon.js

var gBodyElement;
var prefs;
var gBackgroundImage;

// Initialize in case we can't get them from prefs???
const defaultTextColor="#000000";
const defaultLinkColor="#000099";
const defaultActiveColor="#000099";
const defaultVisitedColor="#990099";
const defaultBackgroundColor="#FFFFFF";
const styleStr =       "style";
const textStr =        "text";
const linkStr =        "link";
const vlinkStr =       "vlink";
const alinkStr =       "alink";
const bgcolorStr =     "bgcolor";
const backgroundStr =  "background";
const cssColorStr = "color";
const cssBackgroundColorStr = "background-color";
const cssBackgroundImageStr = "background-image";
const colorStyle =     cssColorStr + ": ";
const backColorStyle = cssBackgroundColorStr + ": ";
const backImageStyle = "; " + cssBackgroundImageStr + ": url(";

var customTextColor;
var customLinkColor;
var customActiveColor;
var customVisitedColor;
var customBackgroundColor;
var previewBGColor;
var gHaveDocumentUrl = false;

// dialog initialization code
function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }

  gDialog.ColorPreview = document.getElementById("ColorPreview");
  gDialog.NormalText = document.getElementById("NormalText");
  gDialog.LinkText = document.getElementById("LinkText");
  gDialog.ActiveLinkText = document.getElementById("ActiveLinkText");
  gDialog.VisitedLinkText = document.getElementById("VisitedLinkText");
  gDialog.PageColorGroup = document.getElementById("PageColorGroup");
  gDialog.DefaultColorsRadio = document.getElementById("DefaultColorsRadio");
  gDialog.CustomColorsRadio = document.getElementById("CustomColorsRadio");
  gDialog.BackgroundImageInput = document.getElementById("BackgroundImageInput");

  try {
    gBodyElement = editor.rootElement;
  } catch (e) {}

  if (!gBodyElement)
  {
    dump("Failed to get BODY element!\n");
    window.close();
  }

  // Set element we will edit
  globalElement = gBodyElement.cloneNode(false);

  // Initialize default colors from browser prefs
  var browserColors = GetDefaultBrowserColors();
  if (browserColors)
  {
    // Use author's browser pref colors passed into dialog
    defaultTextColor = browserColors.TextColor;
    defaultLinkColor = browserColors.LinkColor;
    defaultActiveColor = browserColors.ActiveLinkColor;
    defaultVisitedColor =  browserColors.VisitedLinkColor;
    defaultBackgroundColor=  browserColors.BackgroundColor;
  }

  // We only need to test for this once per dialog load
  gHaveDocumentUrl = GetDocumentBaseUrl();

  InitDialog();

  gDialog.PageColorGroup.focus();

  SetWindowLocation();
}

function InitDialog()
{
  // Get image from document
  gBackgroundImage = GetHTMLOrCSSStyleValue(globalElement, backgroundStr, cssBackgroundImageStr);
  if (/url\((.*)\)/.test( gBackgroundImage ))
    gBackgroundImage = RegExp.$1;

  gDialog.BackgroundImageInput.value = gBackgroundImage;

  if (gBackgroundImage)
    gDialog.ColorPreview.setAttribute(styleStr, backImageStyle+gBackgroundImage+");");

  SetRelativeCheckbox();

  customTextColor        = GetHTMLOrCSSStyleValue(globalElement, textStr, cssColorStr);
  customTextColor        = ConvertRGBColorIntoHEXColor(customTextColor);
  customLinkColor        = globalElement.getAttribute(linkStr);
  customActiveColor      = globalElement.getAttribute(alinkStr);
  customVisitedColor     = globalElement.getAttribute(vlinkStr);
  customBackgroundColor  = GetHTMLOrCSSStyleValue(globalElement, bgcolorStr, cssBackgroundColorStr);
  customBackgroundColor  = ConvertRGBColorIntoHEXColor(customBackgroundColor);

  var haveCustomColor = 
        customTextColor       ||
        customLinkColor       ||
        customVisitedColor    ||
        customActiveColor     ||
        customBackgroundColor;

  // Set default color explicitly for any that are missing
  // PROBLEM: We are using "windowtext" and "window" for the Windows OS
  //   default color values. This works with CSS in preview window,
  //   but we should NOT use these as values for HTML attributes!

  if (!customTextColor) customTextColor = defaultTextColor;
  if (!customLinkColor) customLinkColor = defaultLinkColor;
  if (!customActiveColor) customActiveColor = defaultActiveColor;
  if (!customVisitedColor) customVisitedColor = defaultVisitedColor;
  if (!customBackgroundColor) customBackgroundColor = defaultBackgroundColor;

  if (haveCustomColor)
  {
    // If any colors are set, then check the "Custom" radio button
    gDialog.PageColorGroup.selectedItem = gDialog.CustomColorsRadio;
    UseCustomColors();
  }
  else 
  {
    gDialog.PageColorGroup.selectedItem = gDialog.DefaultColorsRadio;
    UseDefaultColors();
  }
}

function GetColorAndUpdate(ColorWellID)
{
  // Only allow selecting when in custom mode
  if (!gDialog.CustomColorsRadio.selected) return;

  var colorWell = document.getElementById(ColorWellID);
  if (!colorWell) return;

  // Don't allow a blank color, i.e., using the "default"
  var colorObj = { NoDefault:true, Type:"", TextColor:0, PageColor:0, Cancel:false };

  switch( ColorWellID )
  {
    case "textCW":
      colorObj.Type = "Text";
      colorObj.TextColor = customTextColor;
      break;
    case "linkCW":
      colorObj.Type = "Link";
      colorObj.TextColor = customLinkColor;
      break;
    case "activeCW":
      colorObj.Type = "ActiveLink";
      colorObj.TextColor = customActiveColor;
      break;
    case "visitedCW":
      colorObj.Type = "VisitedLink";
      colorObj.TextColor = customVisitedColor;
      break;
    case "backgroundCW":
      colorObj.Type = "Page";
      colorObj.PageColor = customBackgroundColor;
      break;
  }

  window.openDialog("chrome://editor/content/EdColorPicker.xul", "_blank", "chrome,close,titlebar,modal", "", colorObj);

  // User canceled the dialog
  if (colorObj.Cancel)
    return;

  var color = "";
  switch( ColorWellID )
  {
    case "textCW":
      color = customTextColor = colorObj.TextColor;
      break;
    case "linkCW":
      color = customLinkColor = colorObj.TextColor;
      break;
    case "activeCW":
      color = customActiveColor = colorObj.TextColor;
      break;
    case "visitedCW":
      color = customVisitedColor = colorObj.TextColor;
      break;
    case "backgroundCW":
      color = customBackgroundColor = colorObj.BackgroundColor;
      break;
  }

  setColorWell(ColorWellID, color); 
  SetColorPreview(ColorWellID, color);
}

function SetColorPreview(ColorWellID, color)
{
  switch( ColorWellID )
  {
    case "textCW":
      gDialog.NormalText.setAttribute(styleStr,colorStyle+color);
      break;
    case "linkCW":
      gDialog.LinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "activeCW":
      gDialog.ActiveLinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "visitedCW":
      gDialog.VisitedLinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "backgroundCW":
      // Must combine background color and image style values
      var styleValue = backColorStyle+color;
      if (gBackgroundImage)
        styleValue += ";"+backImageStyle+gBackgroundImage+");";

      gDialog.ColorPreview.setAttribute(styleStr,styleValue);
      previewBGColor = color;
      break;
  }
}

function UseCustomColors()
{
  SetElementEnabledById("TextButton", true);
  SetElementEnabledById("LinkButton", true);
  SetElementEnabledById("ActiveLinkButton", true);
  SetElementEnabledById("VisitedLinkButton", true);
  SetElementEnabledById("BackgroundButton", true);
  SetElementEnabledById("Text", true);
  SetElementEnabledById("Link", true);
  SetElementEnabledById("Active", true);
  SetElementEnabledById("Visited", true);
  SetElementEnabledById("Background", true);

  SetColorPreview("textCW",       customTextColor);
  SetColorPreview("linkCW",       customLinkColor);
  SetColorPreview("activeCW",     customActiveColor);
  SetColorPreview("visitedCW",    customVisitedColor);
  SetColorPreview("backgroundCW", customBackgroundColor);

  setColorWell("textCW",          customTextColor);
  setColorWell("linkCW",          customLinkColor);
  setColorWell("activeCW",        customActiveColor);
  setColorWell("visitedCW",       customVisitedColor);
  setColorWell("backgroundCW",    customBackgroundColor);
}

function UseDefaultColors()
{
  SetColorPreview("textCW",       defaultTextColor);
  SetColorPreview("linkCW",       defaultLinkColor);
  SetColorPreview("activeCW",     defaultActiveColor);
  SetColorPreview("visitedCW",    defaultVisitedColor);
  SetColorPreview("backgroundCW", defaultBackgroundColor);

  // Setting to blank color will remove color from buttons,
  setColorWell("textCW",       "");
  setColorWell("linkCW",       "");
  setColorWell("activeCW",     "");
  setColorWell("visitedCW",    "");
  setColorWell("backgroundCW", "");

  // Disable color buttons and labels
  SetElementEnabledById("TextButton", false);
  SetElementEnabledById("LinkButton", false);
  SetElementEnabledById("ActiveLinkButton", false);
  SetElementEnabledById("VisitedLinkButton", false);
  SetElementEnabledById("BackgroundButton", false);
  SetElementEnabledById("Text", false);
  SetElementEnabledById("Link", false);
  SetElementEnabledById("Active", false);
  SetElementEnabledById("Visited", false);
  SetElementEnabledById("Background", false);
}

function chooseFile()
{
  // Get a local image file, converted into URL format
  var fileName = GetLocalFileURL("img");
  if (fileName)
  {
    // Always try to relativize local file URLs
    if (gHaveDocumentUrl)
      fileName = MakeRelativeUrl(fileName);

    gDialog.BackgroundImageInput.value = fileName;

    SetRelativeCheckbox();

    ValidateAndPreviewImage(true);
  }
  SetTextboxFocus(gDialog.BackgroundImageInput);
}

function ChangeBackgroundImage()
{
  // Don't show error message for image while user is typing
  ValidateAndPreviewImage(false);
  SetRelativeCheckbox();
}

function ValidateAndPreviewImage(ShowErrorMessage)
{
  // First make a string with just background color
  var styleValue = backColorStyle+previewBGColor+";";

  var retVal = true;
  var image = TrimString(gDialog.BackgroundImageInput.value);
  if (image)
  {
    gBackgroundImage = image;

    // Display must use absolute URL if possible
    var displayImage = gHaveDocumentUrl ? MakeAbsoluteUrl(image) : image;
    styleValue += backImageStyle+displayImage+");";
  }
  else gBackgroundImage = null;

  // Set style on preview (removes image if not valid)
  gDialog.ColorPreview.setAttribute(styleStr, styleValue);

  // Note that an "empty" string is valid
  return retVal;
}

function ValidateData()
{
  var editor = GetCurrentEditor();
  try {
    // Colors values are updated as they are picked, no validation necessary
    if (gDialog.DefaultColorsRadio.selected)
    {
      editor.removeAttributeOrEquivalent(globalElement, textStr, true);
      globalElement.removeAttribute(linkStr);
      globalElement.removeAttribute(vlinkStr);
      globalElement.removeAttribute(alinkStr);
      editor.removeAttributeOrEquivalent(globalElement, bgcolorStr, true);
    }
    else
    {
      //Do NOT accept the CSS "WindowsOS" color strings!
      // Problem: We really should try to get the actual color values
      //  from windows, but I don't know how to do that!
      var tmpColor = customTextColor.toLowerCase();
      if (tmpColor != "windowtext")
        editor.setAttributeOrEquivalent(globalElement, textStr,
                                        customTextColor, true);
      else
        editor.removeAttributeOrEquivalent(globalElement, textStr, true);

      tmpColor = customBackgroundColor.toLowerCase();
      if (tmpColor != "window")
        editor.setAttributeOrEquivalent(globalElement, bgcolorStr, customBackgroundColor, true);
      else
        editor.removeAttributeOrEquivalent(globalElement, bgcolorStr, true);

      globalElement.setAttribute(linkStr,    customLinkColor);
      globalElement.setAttribute(vlinkStr,   customVisitedColor);
      globalElement.setAttribute(alinkStr,   customActiveColor);
    }

    if (ValidateAndPreviewImage(true))
    {
      // A valid image may be null for no image
      if (gBackgroundImage)
        globalElement.setAttribute(backgroundStr, gBackgroundImage);
      else
        editor.removeAttributeOrEquivalent(globalElement, backgroundStr, true);
  
      return true;
    }  
  } catch (e) {}
  return false;
}

function onAccept()
{
  if (ValidateData())
  {
    // Copy attributes to element we are changing
    try {
      GetCurrentEditor().cloneAttributes(gBodyElement, globalElement);
    } catch (e) {}

    SaveWindowLocation();
    return true; // do close the window
  }
  return false;
}
