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
var backImageStyle = " background-image: url(";

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  dialog = new Object;
  if (!dialog)
  {
    dump("Failed to create dialog object!!!\n");
    window.close();
  }

  dialog.ColorPreview = document.getElementById("ColorPreview");
  dialog.NormalText = document.getElementById("NormalText");
  dialog.LinkText = document.getElementById("LinkText");
  dialog.ActiveLinkText = document.getElementById("ActiveLinkText");
  dialog.VisitedLinkText = document.getElementById("VisitedLinkText");
  dialog.DefaultColorsRadio = document.getElementById("DefaultColorsRadio");
  dialog.CustomColorsRadio = document.getElementById("CustomColorsRadio");
  dialog.BackgroundImageInput = document.getElementById("BackgroundImageInput");

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
  var prefs = GetPrefs();
  if (prefs)
  {
    // This doesn't necessarily match what appears in the page
    // It is complicated by browser.display.use_document_colors
    // TODO: WE MUST FORCE WINDOW TO USE DOCUMENT COLORS!!!
    //       How do we do that without changing browser prefs?
    var useDocumentColors =    prefs.GetBoolPref("browser.display.use_document_colors");
    if (useDocumentColors)
    {
      // How do I get current colors as show in page?
    } else {
      try {
        // Use author's browser pref colors
        defaultTextColor =         prefs.CopyCharPref("browser.display.foreground_color");
        defaultLinkColor =         prefs.CopyCharPref("browser.anchor_color");
        // Note: Browser doesn't store a value for ActiveLinkColor
        defaultActiveColor = defaultLinkColor;
        defaultVisitedColor =  prefs.CopyCharPref("browser.visited_color");
        defaultBackgroundColor=    prefs.CopyCharPref("browser.display.background_color");
      }
      catch (ex) {
        dump("Failed getting browser colors from prefs\n");
      }
    }
  }
  InitDialog();

  if (dialog.DefaultColorsRadio.checked)
    dialog.DefaultColorsRadio.focus();
  else
    dialog.CustomColorsRadio.focus();

  SetWindowLocation();
}

function InitDialog()
{
  // Get image from document
  backgroundImage = globalElement.getAttribute(backgroundStr);
  if (backgroundImage.length > 0)
  {
    dialog.BackgroundImageInput.value = backgroundImage;
    dialog.ColorPreview.setAttribute(styleStr, backImageStyle+backgroundImage+");");
  }

  customTextColor        = globalElement.getAttribute(textStr);
  customLinkColor        = globalElement.getAttribute(linkStr);
  customActiveColor      = globalElement.getAttribute(alinkStr);
  customVisitedColor     = globalElement.getAttribute(vlinkStr);
  customBackgroundColor  = globalElement.getAttribute(bgcolorStr);

  if (customTextColor       ||
      customLinkColor       ||
      customVisitedColor    ||
      customActiveColor     ||
      customBackgroundColor)
  {
    // Set default color explicitly for any that are missing
    if (!customTextColor) customTextColor = defaultTextColor;
    if (!customLinkColor) customLinkColor = defaultLinkColor;
    if (!customActiveColor) customActiveColor = defaultActiveColor;
    if (!customVisitedColor) customVisitedColor = defaultVisitedColor;
    if (!customBackgroundColor) customBackgroundColor = defaultBackgroundColor;

    // If any colors are set, then check the "Custom" radio button
    dialog.CustomColorsRadio.checked = true;
    UseCustomColors();
  }
  else 
  {
    dialog.DefaultColorsRadio.checked = true;
    UseDefaultColors();
  }
}

function GetColorAndUpdate(ColorWellID)
{
  // Only allow selecting when in custom mode
  if (!dialog.CustomColorsRadio.checked) return;

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

  color = "";
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
  
  // Setting a color automatically changes into UseCustomColors mode
  //dialog.CustomColorsRadio.checked = true;
}

function SetColorPreview(ColorWellID, color)
{
  switch( ColorWellID )
  {
    case "textCW":
      dialog.NormalText.setAttribute(styleStr,colorStyle+color);
      break;
    case "linkCW":
      dialog.LinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "activeCW":
      dialog.ActiveLinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "visitedCW":
      dialog.VisitedLinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "backgroundCW":
      // Must combine background color and image style values
      styleValue = backColorStyle+color;
      if (backgroundImage.length > 0)
        styleValue += ";"+backImageStyle+backgroundImage+");";

      dialog.ColorPreview.setAttribute(styleStr,styleValue);
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

  // Disable color buttons
  SetElementEnabledById("TextButton", false);
  SetElementEnabledById("LinkButton", false);
  SetElementEnabledById("ActiveLinkButton", false);
  SetElementEnabledById("VisitedLinkButton", false);
  SetElementEnabledById("BackgroundButton", false);
}

function chooseFile()
{
  // Get a local image file, converted into URL format
  fileName = GetLocalFileURL("img");
  if (fileName && fileName != "")
  {
    dialog.BackgroundImageInput = filename;
    ValidateAndPreviewImage(true);
  }
  SetTextfieldFocus(dialog.BackgroundImageInput);
}

function ChangeBackgroundImage()
{
  // Don't show error message for image while user is typing
  ValidateAndPreviewImage(false);
}

function ValidateAndPreviewImage(ShowErrorMessage)
{
  // First make a string with just background color
  var styleValue = backColorStyle+dialog.backgroundColor+";";

  var image = dialog.BackgroundImageInput.value.trimString();
  if (image.length > 0)
  {
    if (IsValidImage(image))
    {
      backgroundImage = image;
      // Append image style
      styleValue += backImageStyle+backgroundImage+");";
    }
    else
    {
      backgroundImage = null;
      if (ShowErrorMessage)
      {
        SetTextfieldFocus(dialog.BackgroundImageInput);
        // Tell user about bad image
        ShowInputErrorMessage(GetString("MissingImageError"));
      }
      return false;
    }
  }
  else backgroundImage = null;

  // Set style on preview (removes image if not valid)
  dialog.ColorPreview.setAttribute(styleStr, styleValue);

  // Note that an "empty" string is valid
  return true;
}

function ValidateData()
{
  // Colors values are updated as they are picked, no validation necessary
  if (dialog.DefaultColorsRadio.checked)
  {
    globalElement.removeAttribute(textStr);
    globalElement.removeAttribute(linkStr);
    globalElement.removeAttribute(vlinkStr);
    globalElement.removeAttribute(alinkStr);
    globalElement.removeAttribute(bgcolorStr);
  }
  else
  {
    globalElement.setAttribute(textStr,    customTextColor);
    globalElement.setAttribute(linkStr,    customLinkColor);
    globalElement.setAttribute(vlinkStr,   customVisitedColor);
    globalElement.setAttribute(alinkStr,   customActiveColor);
    globalElement.setAttribute(bgcolorStr, customBackgroundColor);
  }

  if (ValidateAndPreviewImage(true))
  {
    // A valid image may be null for no image
    if (backgroundImage)
    {
      globalElement.setAttribute(backgroundStr, backgroundImage);
    } else {
      globalElement.removeAttribute(backgroundStr);
      return false;
    }
  }  
  return true;
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
