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


//Cancel() is in EdDialogCommon.js
var insertNew = true;
var tagname = "TAG NAME"
var dialog;
var LastPickedColor = "";
var ColorType; // = "Text";
var TableOrCell = false;
var LastPickedIsDefault = true;
var gLocation;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell() || !window.arguments[1])
  {
    dump("EdColorPicker: Failed to get EditorShell or arguments[1]\n");
    return;
  }

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
  //    Text, or background: Page, Table, or Cell
  ColorType = window.arguments[1].Type;

  dialog.ColorInput.value = "";
  var color = "";

  // window.arguments[1] is object to set initial and return color
  switch (ColorType)
  {
    case "Page":
      window.title = GetString("PageBackgroundColor");
      if (window.arguments[1].PageColor)
        color = window.arguments[1].PageColor;
      break;
    case "Table":
      window.title = GetString("TableBackgroundColor");
      if (window.arguments[1].TableColor)
        color = window.arguments[1].TableColor;
      break;
    case "Cell":
      window.title = GetString("CellBackgroundColor");
      if (window.arguments[1].CellColor)
        color = window.arguments[1].CellColor;
      break;
    case "TableOrCell":
      TableOrCell = true;
      document.getElementById("TableOrCellGroup").setAttribute("collapsed", "false");
      window.title = GetString("TableOrCellColor");
      if (window.arguments[1].TableColor)
      {
        color = window.arguments[1].TableColor;
        dialog.TableRadio.checked = true;
      }
      else 
      {
        if (window.arguments[1].CellColor)
          color = window.arguments[1].CellColor;

        dialog.CellRadio.checked = true;
      }
      break;
    default:
      window.title = GetString("TextColor");
      if (window.arguments[1].TextColor)
        color = window.arguments[1].TextColor;
      break;
  }

  SetCurrentColor(color)

  if (ColorType == "Text")
    LastPickedColor = dialog.LastPickedColor.getAttribute("LastTextColor");
  else
    LastPickedColor = dialog.LastPickedColor.getAttribute("LastBackgroundColor");

dump("LastPickedColor = "+LastPickedColor+"|\n");

  dialog.LastPickedColor.setAttribute("style","background-color: "+LastPickedColor);

  doSetOKCancel(onOK, onCancel);

  // Set method to detect clicking on OK button
  //  so we don't get fooled by changing "default" behavior
  dialog.Ok.setAttribute("onclick", "onOKClick()");

  // Make the "Last-picked" the default button
  //  until the user selects a color
  
  dialog.Ok.removeAttribute("default");
  dialog.LastPickedButton.setAttribute("default","true");

  SetTextfieldFocus(dialog.ColorInput);

  SetWindowLocation();
}

function ChangePalette(palette)
{
  dialog.ColorPicker.setAttribute("palettename", palette);
  window.sizeToContent();
}

function SelectColor()
{
  var color = dialog.ColorPicker.getAttribute("color");
  if (color.length > 0)
  {
    SetCurrentColor(color);
    SetDefaultToOk();
  }
}

function RemoveColor()
{
  SetCurrentColor("");
  dialog.ColorInput.focus();
  SetDefaultToOk();
}

function SelectLastPickedColor()
{
  SetCurrentColor(LastPickedColor);
  dialog.ColorInput.focus();
  SetDefaultToOk();
}

function SetCurrentColor(color)
{
  // TODO: Validate color?
  dialog.ColorInput.value = color.trimString().toLowerCase();
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

function onOKClick()
{
  // Clicking on OK should not use LastPickedColor automatically
  //  as using Enter ("default") key does
  LastPickedIsDefault = false;
  onOK();
  window.close();
}

function onOK()
{
  var color;
  if (LastPickedIsDefault)
    color = LastPickedColor;
  else
    color = dialog.ColorInput.value;

  color = color.trimString().toLowerCase();
  // TODO: Validate the color string!

dump("ColorPicker onOK: color = "+color+"|\n");

  // Set return values and save in persistent color attributes
  if (ColorType == "Text")
  {
    window.arguments[1].TextColor = color;
    if (color.length > 0)
      dialog.LastPickedColor.setAttribute("LastTextColor", color);
  }
  else
  {
    window.arguments[1].BackgroundColor = color;
    if (color.length > 0)
      dialog.LastPickedColor.setAttribute("LastBackgroundColor", color);

    // If table or cell requested, tell caller which element to set on
    if (TableOrCell && dialog.TableRadio.checked)
      window.arguments[1].Type = "Table";
  }
  SaveWindowLocation();

  return true; // do close the window
}
