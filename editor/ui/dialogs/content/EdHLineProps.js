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

var tagName = "hr";
var gHLineElement;
var width;
var height;
var align;
var shading;
const gMaxHRSize = 1000; // This is hard-coded in nsHTMLHRElement::StringToAttribute()

// dialog initialization code
function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }
  try {
    // Get the selected horizontal line
    gHLineElement = editor.getSelectedElement(tagName);
  } catch (e) {}

  if (!gHLineElement) {
    // We should never be here if not editing an existing HLine
    window.close();
    return;
  }
  gDialog.heightInput = document.getElementById("height");
  gDialog.widthInput = document.getElementById("width");
  gDialog.leftAlign = document.getElementById("leftAlign");
  gDialog.centerAlign = document.getElementById("centerAlign");
  gDialog.rightAlign = document.getElementById("rightAlign");
  gDialog.alignGroup = gDialog.rightAlign.radioGroup;
  gDialog.shading = document.getElementById("3dShading");
  gDialog.pixelOrPercentMenulist = document.getElementById("pixelOrPercentMenulist");

  // Make a copy to use for AdvancedEdit and onSaveDefault
  globalElement = gHLineElement.cloneNode(false);

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
  // Just to be confusing, "size" is used instead of height because it does
  // not accept % values, only pixels
  var height = GetHTMLOrCSSStyleValue(globalElement, "size", "height")
  if (/px/.test(height)) {
    height = RegExp.leftContext;
  }
  if(!height) {
    height = 2; //Default value
  }

  // We will use "height" here and in UI
  gDialog.heightInput.value = height;

  // Get the width attribute of the element, stripping out "%"
  // This sets contents of menulist (adds pixel and percent menuitems elements)
  gDialog.widthInput.value = InitPixelOrPercentMenulist(globalElement, gHLineElement, "width","pixelOrPercentMenulist");

  var marginLeft  = GetHTMLOrCSSStyleValue(globalElement, "align", "margin-left").toLowerCase();
  var marginRight = GetHTMLOrCSSStyleValue(globalElement, "align", "margin-right").toLowerCase();
  align = marginLeft + " " + marginRight;
  gDialog.leftAlign.checked   = (align == "left left"     || align == "0px auto");
  gDialog.centerAlign.checked = (align == "center center" || align == "auto auto" || align == " ");
  gDialog.rightAlign.checked  = (align == "right right"   || align == "auto 0px");

  if (gDialog.centerAlign.checked) {
    gDialog.alignGroup.selectedItem = gDialog.centerAlign;
  }
  else if (gDialog.rightAlign.checked) {
    gDialog.alignGroup.selectedItem = gDialog.rightAlign;
  }
  else {
    gDialog.alignGroup.selectedItem = gDialog.leftAlign;
  }

  gDialog.shading.checked = !globalElement.hasAttribute("noshade");
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

      var percent;
      var widthInt;
      var heightInt;

      if (width)
      {
        if (/%/.test(width)) {
          percent = true;
          widthInt = Number(RegExp.leftContext);
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
  height = ValidateNumber(gDialog.heightInput, null, 1, gMaxHRSize,
                          globalElement, "size", false);
  if (gValidationError)
    return false;

  width = ValidateNumber(gDialog.widthInput, gDialog.pixelOrPercentMenulist, 1, gMaxPixels, 
                         globalElement, "width", false);
  if (gValidationError)
    return false;

  align = "left";
  if (gDialog.centerAlign.selected) {
    // Don't write out default attribute
    align = "";
  } else if (gDialog.rightAlign.selected) {
    align = "right";
  }
  if (align)
    globalElement.setAttribute("align", align);
  else
    try {
      GetCurrentEditor().removeAttributeOrEquivalent(globalElement, "align", true);
    } catch (e) {}

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
    try {
      GetCurrentEditor().cloneAttributes(gHLineElement, globalElement);
    } catch (e) {}
    return true;
  }
  return false;
}
