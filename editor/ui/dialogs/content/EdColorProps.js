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

var BodyElement;
var prefs;
var backgroundImage;

// Initialize in case we can't get them from prefs???
var defaultTextColor="#000000";
var defaultLinkColor="#000099";
var defaultActiveColor="#000099";
var defaultVisitedColor="#990099";
var defaultBackgroundColor="#FFFFFF";

var customTextColor;
var customLinkColor;
var customActiveColor;
var customVisitedColor;
var customBackgroundColor;
var previewBGColor;

// Strings we use often
var styleStr =       "style";
var textStr =        "text";
var linkStr =        "link";
var vlinkStr =       "vlink";
var alinkStr =       "alink";
var bgcolorStr =     "bgcolor";
var backgroundStr =  "background";
var colorStyle =     "color: ";
var backColorStyle = "background-color: ";
var backImageStyle = "; background-image: url(";
var gHaveDocumentUrl = false;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  gDialog.ColorPreview = document.getElementById("ColorPreview");
  gDialog.NormalText = document.getElementById("NormalText");
  gDialog.LinkText = document.getElementById("LinkText");
  gDialog.ActiveLinkText = document.getElementById("ActiveLinkText");
  gDialog.VisitedLinkText = document.getElementById("VisitedLinkText");
  gDialog.PageColorGroup = document.getElementById("PageColorGroup");
  gDialog.DefaultColorsRadio = document.getElementById("DefaultColorsRadio");
  gDialog.CustomColorsRadio = document.getElementById("CustomColorsRadio");
  gDialog.BackgroundImageInput = document.getElementById("BackgroundImageInput");

  BodyElement = editorShell.editorDocument.body;
  if (!BodyElement)
  {
    dump("Failed to get BODY element!\n");
    window.close();
  }

  // Set element we will edit
  globalElement = BodyElement.cloneNode(false);

  doSetOKCancel(onOK, onCancel);

  // Initialize default colors from browser prefs
  var browserColors = GetDefaultBrowserColors();
  if (browserColors)
  {
    // Use author's browser pref colors passed into dialog
    defaultTextColor = browserColors.TextColor;
    defaultLinkColor = browserColors.LinkColor;
    // Note: Browser doesn't store a value for ActiveLinkColor
    defaultActiveColor = defaultLinkColor;
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
  backgroundImage = globalElement.getAttribute(backgroundStr);
  if (backgroundImage)
  {
    gDialog.BackgroundImageInput.value = backgroundImage;
    gDialog.ColorPreview.setAttribute(styleStr, backImageStyle+backgroundImage+");");
  }

  SetRelativeCheckbox();

  customTextColor        = globalElement.getAttribute(textStr);
  customLinkColor        = globalElement.getAttribute(linkStr);
  customActiveColor      = globalElement.getAttribute(alinkStr);
  customVisitedColor     = globalElement.getAttribute(vlinkStr);
  customBackgroundColor  = globalElement.getAttribute(bgcolorStr);

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

  var colorObj = new Object;
  var colorWell = document.getElementById(ColorWellID);
  if (!colorWell) return;

  // Don't allow a blank color, i.e., using the "default"
  colorObj.NoDefault = true;

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
      if (backgroundImage)
        styleValue += ";"+backImageStyle+backgroundImage+");";

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
    if (IsValidImage(image))
    {
      backgroundImage = image;

      // Display must use absolute URL if possible
      var displayImage = gHaveDocumentUrl ? MakeAbsoluteUrl(image) : image;
      styleValue += backImageStyle+displayImage+");";
    }
    else
    {
      backgroundImage = null;
      if (ShowErrorMessage)
      {
        SetTextboxFocus(gDialog.BackgroundImageInput);
        // Tell user about bad image
        ShowInputErrorMessage(GetString("MissingImageError"));
      }
      retVal = false;
    }
  }
  else backgroundImage = null;

  // Set style on preview (removes image if not valid)
  gDialog.ColorPreview.setAttribute(styleStr, styleValue);

  // Note that an "empty" string is valid
  return retVal;
}

function ValidateData()
{
  // Colors values are updated as they are picked, no validation necessary
  if (gDialog.DefaultColorsRadio.selected)
  {
    globalElement.removeAttribute(textStr);
    globalElement.removeAttribute(linkStr);
    globalElement.removeAttribute(vlinkStr);
    globalElement.removeAttribute(alinkStr);
    globalElement.removeAttribute(bgcolorStr);
  }
  else
  {
    //Do NOT accept the CSS "WindowsOS" color strings!
    // Problem: We really should try to get the actual color values
    //  from windows, but I don't know how to do that!
    var tmpColor = customTextColor.toLowerCase();
    if (tmpColor != "windowtext")
      globalElement.setAttribute(textStr,    customTextColor);
    else
      globalElement.removeAttribute(textStr);

    tmpColor = customBackgroundColor.toLowerCase();
    if (tmpColor != "window")
      globalElement.setAttribute(bgcolorStr, customBackgroundColor);
    else
      globalElement.removeAttribute(bgcolorStr);

    globalElement.setAttribute(linkStr,    customLinkColor);
    globalElement.setAttribute(vlinkStr,   customVisitedColor);
    globalElement.setAttribute(alinkStr,   customActiveColor);
  }

  if (ValidateAndPreviewImage(true))
  {
    // A valid image may be null for no image
    if (backgroundImage)
      globalElement.setAttribute(backgroundStr, backgroundImage);
    else
      globalElement.removeAttribute(backgroundStr);
  
    return true;
  }  
  return false;
}

function onOK()
{
  if (ValidateData())
  {
    // Copy attributes to element we are changing
    editorShell.CloneAttributes(BodyElement, globalElement);

    SaveWindowLocation();
    return true; // do close the window
  }
  return false;
}
