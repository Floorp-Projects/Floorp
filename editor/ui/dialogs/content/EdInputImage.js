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
 * The Original Code is Input Tag Properties Dialog.
 *
 * The Initial Developer of the Original Code is
 * Neil Rashbrook.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Neil Rashbrook <neil@parkwaycc.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// dialog initialization code

function Startup()
{
  var editor = GetCurrentEditor();
  if (!editor)
  {
    window.close();
    return;
  }

  gDialog = {
    inputName:      document.getElementById( "InputName" ),
    inputDisabled:  document.getElementById( "InputDisabled" ),
    inputTabIndex:  document.getElementById( "InputTabIndex" )
  };

  ImageStartup();

  // Get a single selected input element
  var tagName = "input";
  try {
    imageElement = editor.getSelectedElement(tagName);
  } catch (e) {}

  if (imageElement)
  {
    // We found an element and don't need to insert one
    gInsertNewImage = false;
  }
  else
  {
    gInsertNewImage = true;

    // We don't have an element selected,
    //  so create one with default attributes
    try {
      imageElement = editor.createElementWithDefaults(tagName);
    } catch(e) {}

    if (!imageElement )
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
    }
    var imgElement;
    try {
      imgElement = editor.getSelectedElement("img");
    } catch(e) {}

    if (imgElement)
    {
      // We found an image element, convert it to an input type="image"
      var attributes = ["src", "alt", "width", "height", "hspace", "vspace", "border", "align", "usemap", "ismap"];
      for (i in attributes)
        imageElement.setAttribute(attributes[i], imgElement.getAttribute(attributes[i]));
    }
  }

  // Make a copy to use for AdvancedEdit
  globalElement = imageElement.cloneNode(false);

  // We only need to test for this once per dialog load
  gHaveDocumentUrl = GetDocumentBaseUrl();

  InitDialog();

  // Save initial source URL
  gOriginalSrc = gDialog.srcInput.value;

  // By default turn constrain on, but both width and height must be in pixels
  gDialog.constrainCheckbox.checked =
    gDialog.widthUnitsMenulist.selectedIndex == 0 &&
    gDialog.heightUnitsMenulist.selectedIndex == 0;

  SetTextboxFocus(gDialog.inputName);

  SetWindowLocation();
}

function InitDialog()
{
  InitImage();
  gDialog.inputName.value = globalElement.getAttribute("name");
  gDialog.inputDisabled.setAttribute("checked", globalElement.hasAttribute("disabled"));
  gDialog.inputTabIndex.value = globalElement.getAttribute("tabindex");
}

function ValidateData()
{
  if (!ValidateImage())
    return false;
  if (gDialog.inputName.value)
    globalElement.setAttribute("name", gDialog.inputName.value);
  else
    globalElement.removeAttribute("name");
  if (gDialog.inputTabIndex.value)
    globalElement.setAttribute("tabindex", gDialog.inputTabIndex.value);
  else
    globalElement.removeAttribute("tabindex");
  if (gDialog.inputDisabled.checked)
    globalElement.setAttribute("disabled", "");
  else
    globalElement.removeAttribute("disabled");
  globalElement.setAttribute("type", "image");
  return true;
}

function onAccept()
{
  // Show alt text error only once
  // (we don't initialize doAltTextError=true
  //  so Advanced edit button dialog doesn't trigger that error message)
  // Use this now (default = false) so Advanced Edit button dialog doesn't trigger error message
  gDoAltTextError = true;

  if (ValidateData())
  {

    var editor = GetCurrentEditor();
    editor.beginTransaction();

    try {
      if (gRemoveImageMap)
      {
        globalElement.removeAttribute("usemap");
        if (gImageMap)
        {
          editor.deleteNode(gImageMap);
          gInsertNewIMap = true;
          gImageMap = null;
        }
      }
      else if (gImageMap)
      {
        // Assign to map if there is one
        var mapName = gImageMap.getAttribute("name");
        if (mapName != "")
        {
          globalElement.setAttribute("usemap", ("#"+mapName));
          if (globalElement.getAttribute("border") == "")
            globalElement.setAttribute("border", 0);
        }
      }

      if (gInsertNewImage)
      {
        // 'true' means delete the selection before inserting
        // in case were are converting an image to an input type="image"
        editor.insertElementAtSelection(imageElement, true);
      }
      editor.cloneAttributes(imageElement, globalElement);

      // If document is empty, the map element won't insert,
      //  so always insert the image element first
      if (gImageMap && gInsertNewIMap)
      {
        // Insert the ImageMap element at beginning of document
        var body = editor.rootElement;
        editor.setShouldTxnSetSelection(false);
        editor.insertNode(gImageMap, body, 0);
        editor.setShouldTxnSetSelection(true);
      }
    } catch (e) {}

    editor.endTransaction();

    SaveWindowLocation();

    return true;
  }
  return false;
}

