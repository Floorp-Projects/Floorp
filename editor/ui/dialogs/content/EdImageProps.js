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
 *   Pete Collins
 *   Brian King
 *   Ben Goodger
 */

var gPreviewImageHeight = 50;
var StartupCalled = false;

// dialog initialization code

function Startup()
{
  //XXX Very weird! When calling this with an existing image,
  //    we get called twice. That causes dialog layout
  //    to explode to fullscreen!
  if (StartupCalled)
  {
    dump("*** CALLING IMAGE DIALOG Startup() AGAIN! ***\n");
    return;
  }
  StartupCalled = true;

  if (!InitEditorShell())
    return;

  ImageStartup();

  // Get a single selected image element
  var tagName = "img";
  if ("arguments" in window && window.arguments[0])
  {
    imageElement = window.arguments[0];
  }
  else
  {
    // First check for <input type="image">
    imageElement = editorShell.GetSelectedElement("input");
    if (!imageElement || imageElement.getAttribute("type") != "image")
      // Get a single selected image element
      imageElement = editorShell.GetSelectedElement(tagName);
  }

  if (imageElement)
  {
    // We found an element and don't need to insert one
    if (imageElement.hasAttribute("src"))
    {
      gInsertNewImage = false;
      actualWidth  = imageElement.naturalWidth;
      actualHeight = imageElement.naturalHeight;
    }
  }
  else
  {
    gInsertNewImage = true;

    // We don't have an element selected,
    //  so create one with default attributes

    imageElement = editorShell.CreateElementWithDefaults(tagName);
    if (!imageElement)
    {
      dump("Failed to get selected element or create a new one!\n");
      window.close();
      return;
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

  SetTextboxFocus(gDialog.srcInput);

  SetWindowLocation();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{
  InitImage();
}

// Get data from widgets, validate, and set for the global element
//   accessible to AdvancedEdit() [in EdDialogCommon.js]
function ValidateData()
{
  return ValidateImage();
}

function doHelpButton()
{
  openHelp("chrome://help/content/help.xul?image_properties");
  return true;
}
