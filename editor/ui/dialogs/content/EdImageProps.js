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

var gAnchorElement = null;
var gOriginalHref = "";
var gHNodeArray = [];

// dialog initialization code

function Startup()
{
  if (!InitEditorShell())
    return;

  ImageStartup();
  gDialog.hrefInput        = document.getElementById("hrefInput");
  gDialog.makeRelativeLink = document.getElementById("MakeRelativeLink");
  gDialog.showLinkBorder   = document.getElementById("showLinkBorder");
  gDialog.linkTab          = document.getElementById("imageLinkTab");

  // Get a single selected image element
  var tagName = "img";
  if ("arguments" in window && window.arguments[0])
  {
    imageElement = window.arguments[0];
    // We've been called from form field propertes, so we can't insert a link
    gDialog.linkTab.parentNode.removeChild(gDialog.linkTab);
    gDialog.linkTab = null;
  }
  else
  {
    // First check for <input type="image">
    imageElement = editorShell.GetSelectedElement("input");
    if (!imageElement || imageElement.getAttribute("type") != "image") {
      // Get a single selected image element
      imageElement = editorShell.GetSelectedElement(tagName);
      if (imageElement)
        gAnchorElement = editorShell.GetElementOrParentByTagName("href", imageElement);
    }
  }

  if (imageElement)
  {
    // We found an element and don't need to insert one
    if (imageElement.hasAttribute("src"))
    {
      gInsertNewImage = false;
      gActualWidth  = imageElement.naturalWidth;
      gActualHeight = imageElement.naturalHeight;
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
    gAnchorElement = editorShell.GetSelectedElement(tagName);
  }

  // Make a copy to use for AdvancedEdit
  globalElement = imageElement.cloneNode(false);

  // We only need to test for this once per dialog load
  gHaveDocumentUrl = GetDocumentBaseUrl();

  InitDialog();
  if (gAnchorElement)
    gOriginalHref = gAnchorElement.getAttribute("href");
  gDialog.hrefInput.value = gOriginalHref;

  FillLinkMenulist(gDialog.hrefInput, gHNodeArray);
  ChangeLinkLocation();

  // Save initial source URL
  gOriginalSrc = gDialog.srcInput.value;

  // By default turn constrain on, but both width and height must be in pixels
  gDialog.constrainCheckbox.checked =
    gDialog.widthUnitsMenulist.selectedIndex == 0 &&
    gDialog.heightUnitsMenulist.selectedIndex == 0;

  // Start in "Link" tab if 2nd arguement is true
  if (gDialog.linkTab && "arguments" in window && window.arguments[1])
  {
    document.getElementById("TabBox").selectedTab = gDialog.linkTab;
    SetTextboxFocus(gDialog.hrefInput);
  }
  else
    SetTextboxFocus(gDialog.srcInput);

  SetWindowLocation();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{
  InitImage();
  gDialog.showLinkBorder.checked = gDialog.border.value != "0";
}

function ChangeLinkLocation()
{
  SetRelativeCheckbox(gDialog.makeRelativeLink);
  gDialog.showLinkBorder.disabled = !TrimString(gDialog.hrefInput.value);
}

function ToggleShowLinkBorder()
{
  if (gDialog.showLinkBorder.checked)
  {
    if (TrimString(gDialog.border.value) == "0")
      gDialog.border.value = "";
  }
  else
  {
    gDialog.border.value = "0";
  }
}

// Get data from widgets, validate, and set for the global element
//   accessible to AdvancedEdit() [in EdDialogCommon.js]
function ValidateData()
{
  return ValidateImage();
}

function doHelpButton()
{
  openHelp("image_properties");
  return true;
}

function onAccept()
{
  // Use this now (default = false) so Advanced Edit button dialog doesn't trigger error message
  gDoAltTextError = true;

  if (ValidateData())
  {
    if ("arguments" in window && window.arguments[0])
    {
      SaveWindowLocation();
      return true;
    }

    editorShell.BeginBatchChanges();

    try
    {
      if (gRemoveImageMap)
      {
        globalElement.removeAttribute("usemap");
        if (gImageMap)
        {
          editorShell.DeleteElement(gImageMap);
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

        if (gInsertNewIMap)
        {
          editorShell.editorDocument.body.appendChild(gImageMap);
        //editorShell.InsertElementAtSelection(gImageMap, false);
        }
      }

      // Create or remove the link as appropriate
      var href = gDialog.hrefInput.value;
      if (href != gOriginalHref) {
        if (href)
          editorShell.SetTextProperty("a", "href", href);
        else
          editorShell.RemoveTextProperty("href", "");
      }

      // All values are valid - copy to actual element in doc or
      //   element created to insert
      editorShell.CloneAttributes(imageElement, globalElement);
      if (gInsertNewImage)
      {
        // 'true' means delete the selection before inserting
        editorShell.InsertElementAtSelection(imageElement, true);
        // Also move the insertion point out of the link
        if (href)
          setTimeout(editorShell.RemoveTextProperty, 0, "href", "");
      }

      // Check to see if the link was to a heading
      // Do this last because it moves the caret (BAD!)
      var index = gDialog.hrefInput.selectedIndex;
      if (index in gHNodeArray && gHNodeArray[index])
      {
        var anchorNode = editorShell.editorDocument.createElement("a");
        if (anchorNode)
        {
          anchorNode.name = href.substr(1);
          // Remember to use editorShell method so it is undoable!
          editorShell.InsertElement(anchorNode, gHNodeArray[index], 0, false);
        }
      }

      // un-comment to see that inserting image maps does not work!
      /*test = editorShell.CreateElementWithDefaults("map");
      test.setAttribute("name", "testing");
      testArea = editorShell.CreateElementWithDefaults("area");
      testArea.setAttribute("shape", "circle");
      testArea.setAttribute("coords", "86,102,52");
      testArea.setAttribute("href", "test");
      test.appendChild(testArea);
      editorShell.InsertElementAtSelection(test, false);*/
    }
    catch (e)
    {
      dump(e);
    }

    editorShell.EndBatchChanges();

    SaveWindowLocation();
    return true;
  }

  gDoAltTextError = false;

  return false;
}
