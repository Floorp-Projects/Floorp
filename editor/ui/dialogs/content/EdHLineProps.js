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

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  // Get the selected horizontal line
  hLineElement = editorShell.GetSelectedElement(tagName);

  if (!hLineElement) {
    // We should never be here if not editing an existing HLine
    window.close();
    return;
  }
  gDialog.heightInput = document.getElementById("height");
  gDialog.widthInput = document.getElementById("width");
  gDialog.leftAlign = document.getElementById("leftAlign");
  gDialog.centerAlign = document.getElementById("centerAlign");
  gDialog.rightAlign = document.getElementById("rightAlign");
  gDialog.shading = document.getElementById("3dShading");
  gDialog.pixelOrPercentMenulist = document.getElementById("pixelOrPercentMenulist");

  // Make a copy to use for AdvancedEdit and onSaveDefault
  globalElement = hLineElement.cloneNode(false);

  // Initialize control values based on existing attributes
  InitDialog()

  // SET FOCUS TO FIRST CONTROL
  SetTextboxFocus(gDialog.widthInput);

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
  gDialog.heightInput.value = height;

  // Get the width attribute of the element, stripping out "%"
  // This sets contents of menulist (adds pixel and percent menuitems elements)
  gDialog.widthInput.value = InitPixelOrPercentMenulist(globalElement, hLineElement, "width","pixelOrPercentMenulist");

  align = globalElement.getAttribute("align");
  if (align)
    align = align.toLowerCase();

  gDialog.centerAlign.checked = (align == "center" || !align);
  gDialog.rightAlign.checked  = (align == "right");
  gDialog.leftAlign.checked   = (align == "left");

  // This is tricky! Since the "noshade" attribute doesn't always have a value,
  //  we can't use getAttribute to figure out if it's set!
  // This gets the attribute NODE from the attributes NamedNodeMap
  if (globalElement.attributes.getNamedItem("noshade"))
    gDialog.shading.checked = false;
  else
    gDialog.shading.checked = true;

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
      prefs.setIntPref("editor.hrule.align", alignInt);

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

      prefs.setIntPref("editor.hrule.width", widthInt);
      prefs.setBoolPref("editor.hrule.width_percent", percent);
      prefs.setIntPref("editor.hrule.height", heightInt);
      prefs.setBoolPref("editor.hrule.shading", shading);

      // Write the prefs out NOW!
      var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                  .getService(Components.interfaces.nsIPrefService);
      prefService.savePrefFile(null);
    }
	}
}

// Get and validate data from widgets.
// Set attributes on globalElement so they can be accessed by AdvancedEdit()
function ValidateData()
{
  // Height is always pixels
  height = ValidateNumber(gDialog.heightInput, null, 1, maxPixels,
                          globalElement, "size", false);
  if (gValidationError)
    return false;

  width = ValidateNumber(gDialog.widthInput, gDialog.pixelOrPercentMenulist, 1, maxPixels, 
                         globalElement, "width", false);
  if (gValidationError)
    return false;

  align = "left";
  if (gDialog.centerAlign.checked) {
    // Don't write out default attribute
    align = "";
  } else if (gDialog.rightAlign.checked) {
    align = "right";
  }
  if (align)
    globalElement.setAttribute("align", align);
  else
    globalElement.removeAttribute("align");

  if (gDialog.shading.checked) {
    shading = true;
    globalElement.removeAttribute("noshade");
  } else {
    shading = false;
    globalElement.setAttribute("noshade", "noshade");
  }
  return true;
}

function onAccept()
{
  if (ValidateData())
  {
    // Copy attributes from the globalElement to the document element
    editorShell.CloneAttributes(hLineElement, globalElement);
    return true;
  }
  return false;
}
