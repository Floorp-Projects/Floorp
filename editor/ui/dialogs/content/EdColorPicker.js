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
 *   Daniel Glazman (glazman@netscape.com)
 *   Charles Manske (cmanske@netscape.com)
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


//Cancel() is in EdDialogCommon.js
var insertNew = true;
var tagname = "TAG NAME"
var gColor = "";
var LastPickedColor = "";
var ColorType = "Text";
var TextType = false;
var HighlightType = false;
var TableOrCell = false;
var LastPickedIsDefault = true;
var NoDefault = false;
var gColorObj;

// dialog initialization code
function Startup()
{ 
  if (!window.arguments[1])
  {
    dump("EdColorPicker: Missing color object param\n");
    return;
  }

  // window.arguments[1] is object to get initial values and return color data
  gColorObj = window.arguments[1];
  gColorObj.Cancel = false;

  gDialog.ColorPicker      = document.getElementById("ColorPicker");
  gDialog.ColorInput       = document.getElementById("ColorInput");
  gDialog.LastPickedButton = document.getElementById("LastPickedButton");
  gDialog.LastPickedColor  = document.getElementById("LastPickedColor");
  gDialog.CellOrTableGroup = document.getElementById("CellOrTableGroup");
  gDialog.TableRadio       = document.getElementById("TableRadio");
  gDialog.CellRadio        = document.getElementById("CellRadio");
  gDialog.ColorSwatch      = document.getElementById("ColorPickerSwatch");
  gDialog.Ok               = document.documentElement.getButton("accept");

  // The type of color we are setting: 
  //  text: Text, Link, ActiveLink, VisitedLink, 
  //  or background: Page, Table, or Cell
  if (gColorObj.Type)
  {
    ColorType = gColorObj.Type;
    // Get string for dialog title from passed-in type 
    //   (note constraint on editor.properties string name)
    var prefs = GetPrefs();
    var IsCSSPrefChecked = prefs.getBoolPref("editor.use_css");

    if (GetCurrentEditor())
    {
      if (ColorType == "Page" && IsCSSPrefChecked && IsHTMLEditor())
        document.title = GetString("BlockColor");
      else
        document.title = GetString(ColorType + "Color");
    }
  }

  gDialog.ColorInput.value = "";
  var tmpColor;
  var haveTableRadio = false;

  switch (ColorType)
  {
    case "Page":
      tmpColor = gColorObj.PageColor;
      if (tmpColor && tmpColor.toLowerCase() != "window")
        gColor = tmpColor;
      break;
    case "Table":
      if (gColorObj.TableColor)
        gColor = gColorObj.TableColor;
      break;
    case "Cell":
      if (gColorObj.CellColor)
        gColor = gColorObj.CellColor;
      break;
    case "TableOrCell":
      TableOrCell = true;
      document.getElementById("TableOrCellGroup").collapsed = false;
      haveTableRadio = true;
      if (gColorObj.SelectedType == "Cell")
      {
        gColor = gColorObj.CellColor;
        gDialog.CellOrTableGroup.selectedItem = gDialog.CellRadio;
        gDialog.CellRadio.focus();
      }
      else
      {
        gColor = gColorObj.TableColor;
        gDialog.CellOrTableGroup.selectedItem = gDialog.TableRadio;
        gDialog.TableRadio.focus();
      }
      break;
    case "Highlight":
      HighlightType = true;
      if (gColorObj.HighlightColor)
        gColor = gColorObj.HighlightColor;
      break;
    default:
      // Any other type will change some kind of text,
      TextType = true;
      tmpColor = gColorObj.TextColor;
      if (tmpColor && tmpColor.toLowerCase() != "windowtext")
        gColor = gColorObj.TextColor;
      break;
  }

  // Set initial color in input field and in the colorpicker
  SetCurrentColor(gColor);
  gDialog.ColorPicker.initColor(gColor);

  // Use last-picked colors passed in, or those persistent on dialog
  if (TextType)
  {
    if ( !("LastTextColor" in gColorObj) || !gColorObj.LastTextColor)
      gColorObj.LastTextColor = gDialog.LastPickedColor.getAttribute("LastTextColor");
    LastPickedColor = gColorObj.LastTextColor;
  }
  else if (HighlightType)
  {
    if ( !("LastHighlightColor" in gColorObj) || !gColorObj.LastHighlightColor)
      gColorObj.LastHighlightColor = gDialog.LastPickedColor.getAttribute("LastHighlightColor");
    LastPickedColor = gColorObj.LastHighlightColor;
  }
  else
  {
    if ( !("LastBackgroundColor" in gColorObj) || !gColorObj.LastBackgroundColor)
      gColorObj.LastBackgroundColor = gDialog.LastPickedColor.getAttribute("LastBackgroundColor");
    LastPickedColor = gColorObj.LastBackgroundColor;
  }
  gDialog.LastPickedColor.setAttribute("style","background-color: "+LastPickedColor);

  // Set method to detect clicking on OK button
  //  so we don't get fooled by changing "default" behavior
  gDialog.Ok.setAttribute("onclick", "SetDefaultToOk()");

  // Make the "Last-picked" the default button
  //  until the user selects a color
  gDialog.Ok.removeAttribute("default");
  gDialog.LastPickedButton.setAttribute("default","true");

  // Caller can prevent user from submitting an empty, i.e., default color
  NoDefault = gColorObj.NoDefault;
  if (NoDefault)
  {
    // Hide the "Default button -- user must pick a color
    document.getElementById("DefaultColorButton").collapsed = true;
  }

  // Set focus to colorpicker if not set to table radio buttons above
  if (!haveTableRadio)
    gDialog.ColorPicker.focus();

  SetWindowLocation();
}

function ChangePalette(palette)
{
  gDialog.ColorPicker.setAttribute("palettename", palette);
  window.sizeToContent();
}

function SelectColor()
{
  var color = gDialog.ColorPicker.color;
  if (color)
    SetCurrentColor(color);
}

function RemoveColor()
{
  SetCurrentColor("");
  gDialog.ColorInput.focus();
  SetDefaultToOk();
}

function SelectColorByKeypress(aEvent)
{
  if (aEvent.charCode == aEvent.DOM_VK_SPACE)
  {
    SelectColor();
    SetDefaultToOk();
  }
}

function SelectLastPickedColor()
{
  SetCurrentColor(LastPickedColor);
  if ( onAccept() )
    //window.close();
    return true;

  return false;
}

function SetCurrentColor(color)
{
  // TODO: Validate color?
  if(!color) color = "";
  gColor = TrimString(color).toLowerCase();
  if (gColor == "mixed")
    gColor = "";
  gDialog.ColorInput.value = gColor;
  SetColorSwatch();
}

function SetColorSwatch()
{
  // TODO: DON'T ALLOW SPACES?
  var color = TrimString(gDialog.ColorInput.value);
  if (color)
  {
    gDialog.ColorSwatch.setAttribute("style",("background-color:"+color));
    gDialog.ColorSwatch.removeAttribute("default");
  }
  else
  {
    gDialog.ColorSwatch.setAttribute("style",("background-color:inherit"));
    gDialog.ColorSwatch.setAttribute("default","true");
  }
}

function SetDefaultToOk()
{
  gDialog.LastPickedButton.removeAttribute("default");
  gDialog.Ok.setAttribute("default","true");
  LastPickedIsDefault = false;
}

function ValidateData()
{
  if (LastPickedIsDefault)
    gColor = LastPickedColor;
  else
    gColor = gDialog.ColorInput.value;
  
  gColor = TrimString(gColor).toLowerCase();

  // TODO: Validate the color string!

  if (NoDefault && !gColor)
  {
    ShowInputErrorMessage(GetString("NoColorError"));
    SetTextboxFocus(gDialog.ColorInput);
    return false;   
  }
  return true;
}

function onAccept()
{
  if (!ValidateData())
    return false;

  // Set return values and save in persistent color attributes
  if (TextType)
  {
    gColorObj.TextColor = gColor;
    if (gColor.length > 0)
    {
      gDialog.LastPickedColor.setAttribute("LastTextColor", gColor);
      gColorObj.LastTextColor = gColor;
    }
  }
  else if (HighlightType)
  {
    gColorObj.HighlightColor = gColor;
    if (gColor.length > 0)
    {
      gDialog.LastPickedColor.setAttribute("LastHighlightColor", gColor);
      gColorObj.LastHighlightColor = gColor;
    }
  }
  else
  {
    gColorObj.BackgroundColor = gColor;
    if (gColor.length > 0)
    {
      gDialog.LastPickedColor.setAttribute("LastBackgroundColor", gColor);
      gColorObj.LastBackgroundColor = gColor;
    }
    // If table or cell requested, tell caller which element to set on
    if (TableOrCell && gDialog.TableRadio.selected)
      gColorObj.Type = "Table";
  }
  SaveWindowLocation();

  return true; // do close the window
}

function onCancelColor()
{
  // Tells caller that user canceled
  gColorObj.Cancel = true;
  SaveWindowLocation();
  return true;
}
