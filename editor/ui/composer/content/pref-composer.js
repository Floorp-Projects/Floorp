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
 *   Stefan Hermes <stefanh@inbox.com>
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

var gPrefService = Components.classes["@mozilla.org/preferences-service;1"]
                             .getService(Components.interfaces.nsIPrefBranch2);
const browserPrefsObserver =
{
  observe: function(aSubject, aTopic, aData)
  {
    if (aTopic != "nsPref:changed" || document.getElementById("editor.use_custom_colors").value)
      return;

    switch (aData)
    {
      case "browser.anchor_color":
        SetColorPreview("linkText", gPrefService.getCharPref(aData));
        break;
      case "browser.active_color":
        SetColorPreview("activeLinkText", gPrefService.getCharPref(aData));
        break;
      case "browser.visited_color":
        SetColorPreview("visitedLinkText", gPrefService.getCharPref(aData));
        break;
      default:
        SetBgAndFgColors(gPrefService.getBoolPref("browser.display.use_system_colors"))
    }
  }
};

function Startup()
{
  // Add browser prefs observers
  gPrefService.addObserver("browser.display.use_system_colors", browserPrefsObserver, false);
  gPrefService.addObserver("browser.display.foreground_color", browserPrefsObserver, false);
  gPrefService.addObserver("browser.display.background_color", browserPrefsObserver, false);
  gPrefService.addObserver("browser.anchor_color", browserPrefsObserver, false);
  gPrefService.addObserver("browser.active_color", browserPrefsObserver, false);
  gPrefService.addObserver("browser.visited_color", browserPrefsObserver, false);

  // Add event listener so we can remove our observers
  window.addEventListener("unload", WindowOnUnload, false);
  UpdateDependent(document.getElementById("editor.use_custom_colors").value);
}

function GetColorAndUpdatePref(aType, aButtonID)
{
  // Don't allow a blank color, i.e., using the "default"
  var colorObj = { NoDefault:true, Type:"", TextColor:0, PageColor:0, Cancel:false };
  var preference = document.getElementById("editor." + aButtonID + "_color");

  if (aButtonID == "background")
    colorObj.PageColor = preference.value;
  else
    colorObj.TextColor = preference.value;

  colorObj.Type = aType;

  window.openDialog("chrome://editor/content/EdColorPicker.xul", "_blank", "chrome,close,titlebar,modal", "", colorObj);

  // User canceled the dialog
  if (colorObj.Cancel)
    return;

  // Update preference with picked color
  if (aType == "Page")
    preference.value = colorObj.BackgroundColor;
  else
    preference.value = colorObj.TextColor;
}

function UpdateDependent(aCustomEnabled)
{
  ToggleElements(aCustomEnabled);

  if (aCustomEnabled)
  { // Set current editor colors on preview and buttons
    SetColors("textCW", "normalText", document.getElementById("editor.text_color").value);
    SetColors("linkCW", "linkText", document.getElementById("editor.link_color").value);
    SetColors("activeCW", "activeLinkText", document.getElementById("editor.active_link_color").value);
    SetColors("visitedCW", "visitedLinkText", document.getElementById("editor.followed_link_color").value);
    SetColors("backgroundCW", "ColorPreview", document.getElementById("editor.background_color").value);
  }
  else
  { // Set current browser colors on preview
    SetBgAndFgColors(gPrefService.getBoolPref("browser.display.use_system_colors"));
    SetColorPreview("linkText", gPrefService.getCharPref("browser.anchor_color"));
    SetColorPreview("activeLinkText", gPrefService.getCharPref("browser.active_color"));
    SetColorPreview("visitedLinkText", gPrefService.getCharPref("browser.visited_color"));
  }
}

function ToggleElements(aCustomEnabled)
{
  var buttons = document.getElementById("color-rows").getElementsByTagName("button");
  
  for (var i = 0; i < buttons.length; i++)
  {
    let isLocked = CheckLocked(buttons[i].id);
    buttons[i].disabled = !aCustomEnabled || isLocked;
    buttons[i].previousSibling.disabled = !aCustomEnabled || isLocked;
    buttons[i].firstChild.setAttribute("default", !aCustomEnabled || isLocked);
  }
}

function CheckLocked(aButtonID)
{
  return document.getElementById("editor." + aButtonID + "_color").locked;
}

// Updates preview and button color when a editor color pref change
function UpdateColors(aColorWellID, aPreviewID, aColor)
{
  // Only show editor colors from prefs if we're in custom mode
  if (!document.getElementById("editor.use_custom_colors").value)
    return;

  SetColors(aColorWellID, aPreviewID, aColor)
}

function SetColors(aColorWellID, aPreviewID, aColor)
{
  SetColorWell(aColorWellID, aColor);
  SetColorPreview(aPreviewID, aColor);
}

function SetColorWell(aColorWellID, aColor)
{
  document.getElementById(aColorWellID).style.backgroundColor = aColor;
}

function SetColorPreview(aPreviewID, aColor)
{
  if (aPreviewID == "ColorPreview")
    document.getElementById(aPreviewID).style.backgroundColor = aColor;
  else
    document.getElementById(aPreviewID).style.color = aColor;
}

function UpdateBgImagePreview(aImage)
{
  var colorPreview = document.getElementById("ColorPreview");
  colorPreview.style.backgroundImage = aImage && "url(" + aImage + ")";
}

// Sets browser background/foreground colors
function SetBgAndFgColors(aSysPrefEnabled)
{
  if (aSysPrefEnabled)
  { // Use system colors
    SetColorPreview("normalText", "windowtext");
    SetColorPreview("ColorPreview", "window");
  }
  else
  {
    SetColorPreview("normalText", gPrefService.getCharPref("browser.display.foreground_color"));
    SetColorPreview("ColorPreview", gPrefService.getCharPref("browser.display.background_color"));
  }
}

function ChooseImageFile()
{
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"]
                     .createInstance(nsIFilePicker);
  var editorBundle = document.getElementById("bundle_editor");
  var title = editorBundle.getString("SelectImageFile");
  fp.init(window, title, nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterImages);
  if (fp.show() == nsIFilePicker.returnOK)
    document.getElementById("editor.default_background_image").value = fp.fileURL.spec;

  var textbox = document.getElementById("backgroundImageInput");
  textbox.focus();
  textbox.select();
}

function WindowOnUnload()
{
  gPrefService.removeObserver("browser.display.use_system_colors", browserPrefsObserver, false);
  gPrefService.removeObserver("browser.display.foreground_color", browserPrefsObserver, false);
  gPrefService.removeObserver("browser.display.background_color", browserPrefsObserver, false);
  gPrefService.removeObserver("browser.anchor_color", browserPrefsObserver, false);
  gPrefService.removeObserver("browser.active_color", browserPrefsObserver, false);
  gPrefService.removeObserver("browser.visited_color", browserPrefsObserver, false);
  window.removeEventListener("unload", WindowOnUnload, false);
}
