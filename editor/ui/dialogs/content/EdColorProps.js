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
 When in UseDefaultColors mode, the color buttons can be used,
 Instead of disabling using color buttons while in UseDefault mode,
  and we switch to UseCustom mode automatically
 This let's user start with default palette as starting point for
  really setting colors
  (2_13 Can't disable color buttons anyway!)
*/

//Cancel() is in EdDialogCommon.js

var BodyElement;
var prefs;
var lastSetBackgroundImage;

// Initialize in case we can't get them from prefs???
var defaultTextColor="#000000";
var defaultLinkColor="#000099";
var defaultVisitedLinkColor="#990099";
var defaultBackgroundColor="#FFFFFF";

// Save
var customTextColor;
var customLinkColor;
var customVisitedColor;
var customActiveColor;
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
  dump(BodyElement+"\n");

  // Set element we will edit
  globalElement = BodyElement.cloneNode(false);

  doSetOKCancel(onOK, onCancel);

  // Initialize default colors from browser prefs
  var prefs = GetPrefs();
  if (prefs)
  {
    dump("Getting browser prefs...\n");

    // This doesn't necessarily match what appears in the page
    // It is complicated by browser.display.use_document_colors
    // TODO: WE MUST FORCE WINDOW TO USE DOCUMENT COLORS!!!
    //       How do we do that without changing browser prefs?
    var useDocumentColors =    prefs.GetBoolPref("browser.display.use_document_colors");
    dump("browser.display.use_document_colors = "+ useDocumentColors+"\n");
    if (useDocumentColors)
    {
      // How do I get current colors as show in page?
    } else {
      try {
        // Use author's browser pref colors
        defaultTextColor =         prefs.CopyCharPref("browser.display.foreground_color");
        defaultLinkColor =         prefs.CopyCharPref("browser.anchor_color");
        // Note: Browser doesn't store a value for ActiveLinkColor
        defaultVisitedLinkColor =  prefs.CopyCharPref("browser.visited_color");
        defaultBackgroundColor=    prefs.CopyCharPref("browser.display.background_color");
      }
      catch (ex) {
        dump("Failed getting browser colors from prefs\n");
      }
    }
    try {
      // Get the last-set background image
      lastSetBackgroundImage = prefs.CopyCharPref("editor.default_background_image");
    }
    catch (ex) {
      dump("Failed getting browser colors from prefs\n");
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
  SetColor("textCW",       globalElement.getAttribute(textStr));
  SetColor("linkCW",       globalElement.getAttribute(linkStr));
  SetColor("visitedCW",    globalElement.getAttribute(vlinkStr));
  SetColor("activeCW",     globalElement.getAttribute(alinkStr));
  SetColor("backgroundCW", globalElement.getAttribute(bgcolorStr));

  if (dialog.textColor ||
      dialog.linkColor ||
      dialog.visitedLinkColor ||
      dialog.activeLinkColor ||
      dialog.backgroundColor)
  {
    // If any colors are set, then check the "Custom" radio button
    dialog.CustomColorsRadio.checked = true;
  } else {
    dialog.DefaultColorsRadio.checked = true;
  }
  
  // Save a copy to use when switching from UseDefault to UseCustom
  SaveCustomColors();

  // Get image from document
  dialog.BackgroundImage = globalElement.getAttribute(backgroundStr);
  if (dialog.BackgroundImage)
  {
    dialog.BackgroundImageInput.value = dialog.BackgroundImage;
    dialog.ColorPreview.setAttribute(backgroundStr, dialog.BackgroundImage);
  }
}


function SetColor(ColorWellID, color)
{
  switch( ColorWellID )
  {
    case "textCW":
      if (!color) color = defaultTextColor;
      dialog.textColor = color;
      dialog.NormalText.setAttribute(styleStr,colorStyle+color);
      break;
    case "linkCW":
      if (!color) color = defaultLinkColor;
      dialog.linkColor = color;
      dialog.LinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "activeCW":
      if (!color) color = defaultLinkColor;
      dialog.activeLinkColor = color;
      dialog.ActiveLinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "visitedCW":
      if (!color) color = defaultVisitedLinkColor;
      dialog.visitedLinkColor = color;
      dialog.VisitedLinkText.setAttribute(styleStr,colorStyle+color);
      break;
    case "backgroundCW":
      if (!color) color = defaultBackgroundColor;
      dialog.backgroundColor = color;
      // Must combine background color and image style values
      styleValue = colorStyle+color;
      if (dialog.backgroundImage > 0)
        styleValue += ";"+backImageStyle+backImageStyle+");";

      dialog.ColorPreview.setAttribute(styleStr,styleValue);
      break;
  }
  setColorWell(ColorWellID, color); 
}

function GetColorAndUpdate(ColorPickerID, ColorWellID, widget)
{
  // Close the colorpicker
  widget.parentNode.closePopup();
  SetColor(ColorWellID, getColor(ColorPickerID));
  
  // Setting a color automatically changes into UseCustomColors mode
  dialog.CustomColorsRadio.checked = true;
}

function SaveCustomColors()
{
  customTextColor = dialog.textColor;     
  customLinkColor = dialog.linkColor;
  customVisitedColor = dialog.visitedLinkColor;
  customActiveColor = dialog.activeLinkColor;
  customBackgroundColor = dialog.backgroundColor;
}

function UseCustomColors()
{
  // Restore from saved colors
  SetColor("textCW",       customTextColor);
  SetColor("linkCW",       customLinkColor);
  SetColor("activeCW",     customActiveColor);
  SetColor("visitedCW",    customVisitedColor);
  SetColor("backgroundCW", customBackgroundColor);
}

function UseDefaultColors()
{
  dump("UseDefaultColors\n");
  // Save colors to use when switching back to UseCustomColors
  SaveCustomColors();

  SetColor("textCW",       defaultTextColor);
  SetColor("linkCW",       defaultLinkColor);
  SetColor("activeCW",     defaultLinkColor); //Browser doesn't store this separately
  SetColor("visitedCW",    defaultVisitedLinkColor);
  SetColor("backgroundCW", defaultBackgroundColor);
}

function chooseFile()
{
  // Get a local file, converted into URL format
  fileName = GetLocalFileURL("img");
  if (fileName && fileName != "")
  {
    dialog.BackgroundImage = fileName;
    dialog.BackgroundImageInput.value = fileName;
    dialog.ColorPreview.setAttribute(backgroundStr, fileName);
    // First make a string with just background color
    var styleValue = colorStyle+dialog.backgroundColor+";";
    if (ValidateImage())
    {
      // Append image style
      styleValue += backImageStyle+dialog.BackgroundImage+");";
    }
dump(styleValue+"=style value when setting image\n")
    // Set style on preview (removes image if not checked or not valid)
    dialog.ColorPreview.setAttribute(styleStr, styleValue);
  }
  
  // Put focus into the input field
  SetTextfieldFocus(dialog.BackgroundImageInput);
}

function ValidateImage()
{
  var image = dialog.BackgroundImageInput.value.trimString();
  if (image && image != "")
  {
    if (IsValidImage(image))
    {
      dialog.BackgroundImage = image;
      return true;
    } else {
      dialog.BackgroundImage = null;
    }
  }
  SetTextfieldFocus(dialog.BackgroundImageInput);
  ShowInputErrorMessage(GetString("MissingImageError"));

  return false;
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
    globalElement.setAttribute(textStr,    dialog.textColor);
    globalElement.setAttribute(linkStr,    dialog.linkColor);
    globalElement.setAttribute(vlinkStr,   dialog.visitedLinkColor);
    globalElement.setAttribute(alinkStr,   dialog.activeLinkColor);
    globalElement.setAttribute(bgcolorStr, dialog.backgroundColor);
  }

  if (ValidateImage())
  {
    globalElement.setAttribute(backgroundStr, dialog.BackgroundImage);
  } else {
    globalElement.removeAttribute(backgroundStr);
    return false;
  }
  
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    // Save image for future editing
    if (prefs && dialog.BackgroundImage)
    {
      dump("Saving default background image in prefs\n");
      prefs.SetCharPref("editor.default_background_image", dialog.BackgroundImage);
    }

    // Copy attributes to element we are changing
    editorShell.CloneAttributes(BodyElement, globalElement);

    SaveWindowLocation();
    return true; // do close the window
  }
  return false;
}
