/*  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */


//Cancel() is in EdDialogCommon.js
var insertNew = true;
var tagname = "TAG NAME"
var dialog;
var color = "";
var LastPickedColor = "";
var ColorType = "Text";
var TextType = false;
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

  // Create dialog object to store controls for easy access
  dialog = new Object;

  dialog.ColorPicker      = document.getElementById("ColorPicker");
  dialog.ColorInput       = document.getElementById("ColorInput");
  dialog.LastPickedButton = document.getElementById("LastPickedButton");
  dialog.LastPickedColor  = document.getElementById("LastPickedColor");
  dialog.TableRadio       = document.getElementById("TableRadio");
  dialog.CellRadio        = document.getElementById("CellRadio");
  dialog.Ok               = document.getElementById("ok");
  dialog.ColorSwatch      = document.getElementById("ColorPickerSwatch");
  
  // The type of color we are setting: 
  //  text: Text, Link, ActiveLink, VisitedLink, 
  //  or background: Page, Table, or Cell
  if (gColorObj.Type)
  {
    ColorType = gColorObj.Type;
    // Get string for dialog title from passed-in type 
    //   (note constraint on editor.properties string name)
    window.title = GetString(ColorType+"Color");
  }

  if (!window.title)
    window.title = GetString("Color");


  dialog.ColorInput.value = "";
  var tmpColor;
  var haveTableRadio = false;

  switch (ColorType)
  {
    case "Page":
      tmpColor = gColorObj.PageColor;
      if (tmpColor && tmpColor.toLowerCase() != "window")
        color = tmpColor;
      break;
    case "Table":
      if (gColorObj.TableColor)
        color = gColorObj.TableColor;
      break;
    case "Cell":
      if (gColorObj.CellColor)
        color = gColorObj.CellColor;
      break;
    case "TableOrCell":
      TableOrCell = true;
      document.getElementById("TableOrCellGroup").setAttribute("collapsed", "false");
      haveTableRadio = true;
      if (gColorObj.TableColor)
      {
        color = gColorObj.TableColor;
        dialog.TableRadio.checked = true;
        dialog.TableRadio.focus();
      }
      else 
      {
        color = gColorObj.CellColor;
        dialog.CellRadio.checked = true;
        dialog.CellRadio.focus();
      }
      break;
    default:
      // Any other type will change some kind of text,
      TextType = true;
      tmpColor = gColorObj.TextColor;
      if (tmpColor && tmpColor.toLowerCase() != "windowtext")
        color = gColorObj.TextColor;
      break;
  }

  // Set initial color in input field and in the colorpicker
  SetCurrentColor(color);
  dialog.ColorPicker.initColor(color);

  // Use last-picked colors passed in, or those persistent on dialog
  if (TextType)
  {
    if ( !("LastTextColor" in gColorObj) || !gColorObj.LastTextColor)
      gColorObj.LastTextColor = dialog.LastPickedColor.getAttribute("LastTextColor");
    LastPickedColor = gColorObj.LastTextColor;
  }
  else
  {
    if ( !("LastBackgroundColor" in gColorObj) || !gColorObj.LastBackgroundColor)
      gColorObj.LastBackgroundColor = dialog.LastPickedColor.getAttribute("LastBackgroundColor");
    LastPickedColor = gColorObj.LastBackgroundColor;
  }
  dialog.LastPickedColor.setAttribute("style","background-color: "+LastPickedColor);

  doSetOKCancel(onOK, onCancelColor);

  // Set method to detect clicking on OK button
  //  so we don't get fooled by changing "default" behavior
  dialog.Ok.setAttribute("onclick", "SetDefaultToOk()");

  // Make the "Last-picked" the default button
  //  until the user selects a color
  dialog.Ok.removeAttribute("default");
  dialog.LastPickedButton.setAttribute("default","true");

  // Caller can prevent user from submitting an empty, i.e., default color
  NoDefault = gColorObj.NoDefault;
  if (NoDefault)
  {
    // Hide the "Default button -- user must pick a color
    document.getElementById("DefaultColorButton").setAttribute("collapsed","true");
  }

  // Set focus to colorpicker if not set to table radio buttons above
  if (!haveTableRadio)
    dialog.ColorPicker.focus();

  SetWindowLocation();
}

function ChangePalette(palette)
{
  dialog.ColorPicker.setAttribute("palettename", palette);
  window.sizeToContent();
}

function SelectColor()
{
  var color = dialog.ColorPicker.color;
  if (color)
    SetCurrentColor(color);
}

function RemoveColor()
{
  SetCurrentColor("");
  dialog.ColorInput.focus();
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
  if ( onOK() )
    window.close();
}

function SetCurrentColor(color)
{
  // TODO: Validate color?
  if(!color) color = "";
  color = color.trimString().toLowerCase();
  if (color == "mixed")
    color = "";
  dialog.ColorInput.value = color;
  SetColorSwatch();
}

function SetColorSwatch()
{
  // TODO: DON'T ALLOW SPACES?
  var color = dialog.ColorInput.value.trimString();
  if (color.length > 0)
  {
    dialog.ColorSwatch.setAttribute("style",("background-color:"+color));
    dialog.ColorSwatch.removeAttribute("default");
  }
  else
  {
    dialog.ColorSwatch.setAttribute("style",("background-color:inherit"));
    dialog.ColorSwatch.setAttribute("default","true");
  }
}

function SetDefaultToOk()
{
  dialog.LastPickedButton.removeAttribute("default");
  dialog.Ok.setAttribute("default","true");
  LastPickedIsDefault = false;
}

function ValidateData()
{
  if (LastPickedIsDefault)
    color = LastPickedColor;
  else
    color = dialog.ColorInput.value;
  
  color = color.trimString().toLowerCase();

  // TODO: Validate the color string!

  if (NoDefault && !color)
  {
    ShowInputErrorMessage(GetString("NoColorError"));
    SetTextboxFocus(dialog.ColorInput);
    return false;   
  }
  return true;
}

function onOK()
{
  if (!ValidateData())
    return false;

  // Set return values and save in persistent color attributes
  if (TextType)
  {
    gColorObj.TextColor = color;
    if (color.length > 0)
    {
      dialog.LastPickedColor.setAttribute("LastTextColor", color);
      gColorObj.LastTextColor = color;
    }
  }
  else
  {
    gColorObj.BackgroundColor = color;
    if (color.length > 0)
    {
      dialog.LastPickedColor.setAttribute("LastBackgroundColor", color);
      gColorObj.LastBackgroundColor = color;
    }
    // If table or cell requested, tell caller which element to set on
    if (TableOrCell && dialog.TableRadio.checked)
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
  window.close();
}
