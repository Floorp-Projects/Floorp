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

var tagName = "hr";
var hLineElement;
var width;
var height;
var align;
var shading;
var dialog;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  // Get the selected horizontal line
  hLineElement = editorShell.GetSelectedElement(tagName);

  if (!hLineElement) {
    // We should never be here if not editing an existing HLine
    window.close();
    return;
  }
  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.heightInput = document.getElementById("height");
  dialog.widthInput = document.getElementById("width");
  dialog.leftAlign = document.getElementById("leftAlign");
  dialog.centerAlign = document.getElementById("centerAlign");
  dialog.rightAlign = document.getElementById("rightAlign");
  dialog.shading = document.getElementById("3dShading");
  dialog.pixelOrPercentMenulist = document.getElementById("pixelOrPercentMenulist");

  // Make a copy to use for AdvancedEdit and onSaveDefault
  globalElement = hLineElement.cloneNode(false);

  // Initialize control values based on existing attributes
  InitDialog()

  // SET FOCUS TO FIRST CONTROL
  SetTextboxFocus(dialog.widthInput);

  // Resize window
  window.sizeToContent();

  SetWindowLocation();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{
  // Just to be confusing, "size" is used instead of height
  var height = globalElement.getAttribute("size");
  if(!height) {
    height = 2; //Default value
  }

  // We will use "height" here and in UI
  dialog.heightInput.value = height;

  // Get the width attribute of the element, stripping out "%"
  // This sets contents of menulist (adds pixel and percent menuitems elements)
  dialog.widthInput.value = InitPixelOrPercentMenulist(globalElement, hLineElement, "width","pixelOrPercentMenulist");

  align = globalElement.getAttribute("align").toLowerCase();

  dialog.centerAlign.checked = (align == "center" || !align);
  dialog.rightAlign.checked  = (align == "right");
  dialog.leftAlign.checked   = (align == "left");

  // This is tricky! Since the "noshade" attribute doesn't always have a value,
  //  we can't use getAttribute to figure out if it's set!
  // This gets the attribute NODE from the attributes NamedNodeMap
  if (globalElement.attributes.getNamedItem("noshade"))
    dialog.shading.checked = false;
  else
    dialog.shading.checked = true;

}

function onSaveDefault()
{
  // "false" means set attributes on the globalElement,
  //   not the real element being edited
  if (ValidateData()) {
    var prefs = GetPrefs();
    if (prefs) {

      var alignInt;
      if (align == "left") {
        alignInt = 0;
      } else if (align == "right") {
        alignInt = 2;
      } else {
        alignInt = 1;
      }
      prefs.SetIntPref("editor.hrule.align", alignInt);

      var percentIndex = width.search(/%/);
      var percent;
      var widthInt;
      var heightInt;

      if (width)
      {
        if (percentIndex > 0) {
          percent = true;
          widthInt = Number(width.substr(0, percentIndex));
        } else {
          percent = false;
          widthInt = Number(width);
        }
      }
      else
      {
        percent = true;
        widthInt = Number(100);
      }

      heightInt = height ? Number(height) : 2;

      prefs.SetIntPref("editor.hrule.width", widthInt);
      prefs.SetBoolPref("editor.hrule.width_percent", percent);
      prefs.SetIntPref("editor.hrule.height", heightInt);
      prefs.SetBoolPref("editor.hrule.shading", shading);

      // Write the prefs out NOW!
      prefs.savePrefFile(null);
    }
	}
}

// Get and validate data from widgets.
// Set attributes on globalElement so they can be accessed by AdvancedEdit()
function ValidateData()
{
  // Height is always pixels
  height = ValidateNumber(dialog.heightInput, null, 1, maxPixels,
                          globalElement, "size", false);
  if (gValidationError)
    return false;

  width = ValidateNumber(dialog.widthInput, dialog.pixelOrPercentMenulist, 1, maxPixels, 
                         globalElement, "width", false);
  if (gValidationError)
    return false;

  align = "left";
  if (dialog.centerAlign.checked) {
    // Don't write out default attribute
    align = "";
  } else if (dialog.rightAlign.checked) {
    align = "right";
  }
  if (align)
    globalElement.setAttribute("align", align);
  else
    globalElement.removeAttribute("align");

  if (dialog.shading.checked) {
    shading = true;
    globalElement.removeAttribute("noshade");
  } else {
    shading = false;
    globalElement.setAttribute("noshade", "noshade");
  }
  return true;
}

function onOK()
{
  if (ValidateData())
  {
    // Copy attributes from the globalElement to the document element
    editorShell.CloneAttributes(hLineElement, globalElement);
    SaveWindowLocation();
    return true;
  }
  return false;
}
