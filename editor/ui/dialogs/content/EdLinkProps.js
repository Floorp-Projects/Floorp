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

var anchorElement = null;
var imageElement = null;
var insertNew = false;
var replaceExistingLink = false;
var insertLinkAtCaret;
var needLinkText = false;
var href;
var newLinkText;
var HNodeArray;
var gHaveNamedAnchors = false;
var gHaveHeadings = false;
var gCanChangeHeadingSelected = true;
var gCanChangeAnchorSelected = true;
var gHaveDocumentUrl = false;

// NOTE: Use "href" instead of "a" to distinguish from Named Anchor
// The returned node is has an "a" tagName
var tagName = "href";

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, onCancel);

  // Message was wrapped in a <label> or <div>, so actual text is a child text node
  gDialog.linkTextCaption     = document.getElementById("linkTextCaption");
  gDialog.linkTextMessage     = document.getElementById("linkTextMessage");
  gDialog.linkTextInput       = document.getElementById("linkTextInput");
  gDialog.hrefInput           = document.getElementById("hrefInput");
  gDialog.NamedAnchorList     = document.getElementById("NamedAnchorList");
  gDialog.HeadingsList        = document.getElementById("HeadingsList");
  gDialog.MoreSection         = document.getElementById("MoreSection");
  gDialog.MoreFewerButton     = document.getElementById("MoreFewerButton");
  gDialog.AdvancedEditSection = document.getElementById("AdvancedEdit");

  var selection = editorShell.editorSelection;
  if (selection)
    dump("There is a selection: collapsed = "+selection.isCollapsed+"\n");
  else
    dump("Failed to get selection\n");

  // See if we have a single selected image
  imageElement = editorShell.GetSelectedElement("img");

  if (imageElement)
  {
    // Get the parent link if it exists -- more efficient than GetSelectedElement()
    anchorElement = editorShell.GetElementOrParentByTagName("href", imageElement);
    if (anchorElement)
    {
      if (anchorElement.childNodes.length > 1)
      {
        // If there are other children, then we want to break
        //  this image away by inserting a new link around it,
        //  so make a new node and copy existing attributes
        anchorElement = anchorElement.cloneNode(false);
        //insertNew = true;
        replaceExistingLink = true;
      }
    }
  }
  else
  {
    // Get an anchor element if caret or
    //   entire selection is within the link.
    anchorElement = editorShell.GetSelectedElement(tagName);

    if (anchorElement)
    {
      // Select the entire link
      editorShell.SelectElement(anchorElement);
      selection = editorShell.editorSelection;
    }
    else
    {
      // If selection starts in a link, but extends beyond it,
      //   the user probably wants to extend existing link to new selection,
      //   so check if either end of selection is within a link
      // POTENTIAL PROBLEM: This prevents user from selecting text in an existing
      //   link and making 2 links. 
      // Note that this isn't a problem with images, handled above

      anchorElement = editorShell.GetElementOrParentByTagName("href", selection.anchorNode);
      if (!anchorElement)
        anchorElement = editorShell.GetElementOrParentByTagName("href", selection.focusNode);

      if (anchorElement)
      {
        // But clone it for reinserting/merging around existing
        //   link that only partially overlaps the selection
        anchorElement = anchorElement.cloneNode(false);
        //insertNew = true;
        replaceExistingLink = true;
      }
    }
  }

  if(!anchorElement)
  {
    // No existing link -- create a new one
    anchorElement = editorShell.CreateElementWithDefaults(tagName);
    insertNew = true;
    // Hide message about removing existing link
    document.getElementById("RemoveLinkMsg").setAttribute("hidden","true");
  }
  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
    return;
  } 

  // We insert at caret only when nothing is selected
  insertLinkAtCaret = selection.isCollapsed;
  
  var selectedText;
  if (insertLinkAtCaret)
  {
    // Groupbox caption:
    gDialog.linkTextCaption.setAttribute("label", GetString("LinkText"));

    // Message above input field:
    gDialog.linkTextMessage.setAttribute("value", GetString("EnterLinkText"));
  }
  else
  {
    if (!imageElement)
    {
      // We get here if selection is exactly around a link node
      // Check if selection has some text - use that first
      selectedText = GetSelectionAsText();
      if (!selectedText) 
      {
        // No text, look for first image in the selection
        var children = anchorElement.childNodes;
        if (children)
        {
          for(var i=0; i < children.length; i++) 
          {
            var nodeName = children.item(i).nodeName.toLowerCase();
            if (nodeName == "img")
            {
              imageElement = children.item(i);
              break;
            }
          }
        }
      }
    }
    // Set "caption" for link source and the source text or image URL
    if (imageElement)
    {
      gDialog.linkTextCaption.setAttribute("label", GetString("LinkImage"));
      // Link source string is the source URL of image
      // TODO: THIS DOESN'T HANDLE MULTIPLE SELECTED IMAGES!
      gDialog.linkTextMessage.setAttribute("value", imageElement.src);
    } else {
      gDialog.linkTextCaption.setAttribute("label", GetString("LinkText"));
      if (selectedText) 
      {
        // Use just the first 60 characters and add "..."
        gDialog.linkTextMessage.setAttribute("value", TruncateStringAtWordEnd(ReplaceWhitespace(selectedText, " "), 60, true));
      } else {
        gDialog.linkTextMessage.setAttribute("value", GetString("MixedSelection"));
      }
    }
  }

  // Make a copy to use for AdvancedEdit and onSaveDefault
  globalElement = anchorElement.cloneNode(false);

  // Get the list of existing named anchors and headings
  FillListboxes();

  // We only need to test for this once per dialog load
  gHaveDocumentUrl = GetDocumentBaseUrl();

  // Set data for the dialog controls
  InitDialog();
  
  // Search for a URI pattern in the selected text
  //  as candidate href
  selectedText = TrimString(selectedText); 
  if (!gDialog.hrefInput.value && TextIsURI(selectedText))
      gDialog.hrefInput.value = selectedText;

  // Set initial focus
  if (insertLinkAtCaret) {
    // We will be using the HREF inputbox, so text message
    SetTextboxFocus(gDialog.linkTextInput);
  } else {
    SetTextboxFocus(gDialog.hrefInput);

    // We will not insert a new link at caret, so remove link text input field
    gDialog.linkTextInput.setAttribute("hidden", "true");
    gDialog.linkTextInput = null;
  }

  InitMoreFewer();
    
  // This sets enable state on OK button
  doEnabling();

  SetWindowLocation();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{
  // Must use getAttribute, not "globalElement.href", 
  //  or foreign chars aren't coverted correctly!
  gDialog.hrefInput.value = globalElement.getAttribute("href");

  // Set "Relativize" checkbox according to current URL state
  SetRelativeCheckbox();
}

function chooseFile()
{
  // Get a local file, converted into URL format
  var fileName = GetLocalFileURL("html, img");
  if (fileName) 
  {
    // Always try to relativize local file URLs
    if (gHaveDocumentUrl)
      fileName = MakeRelativeUrl(fileName);

    gDialog.hrefInput.value = fileName;

    SetRelativeCheckbox();
    doEnabling();
  }
  // Put focus into the input field
  SetTextboxFocus(gDialog.hrefInput);
}

function FillListboxes()
{
  var NamedAnchorNodeList = editorShell.editorDocument.anchors;
  var NamedAnchorCount = NamedAnchorNodeList.length;
  var item;
  if (NamedAnchorCount > 0)
  {
    for (var i = 0; i < NamedAnchorCount; i++)
      AppendStringToTreelist(gDialog.NamedAnchorList, NamedAnchorNodeList.item(i).name);

    gHaveNamedAnchors = true;
  } 
  else 
  {
    // Message to tell user there are none
    item = AppendStringToTreelistById(gDialog.NamedAnchorList, "NoNamedAnchors");
    if (item) item.setAttribute("disabled", "true");
  }
  var firstHeading = true;
  for (var j = 1; j <= 6; j++)
  {
    var headingList = editorShell.editorDocument.getElementsByTagName("h"+String(j));
    if (headingList.length > 0)
    {
      var heading = headingList.item(0);

      // Skip headings that already have a named anchor as their first child
      //  (this may miss nearby anchors, but at least we don't insert another
      //   under the same heading)
      var child = heading.firstChild;
      if (child && child.nodeName == "A" && child.name && (child.name.length>0))
        continue;

      var range = editorShell.editorDocument.createRange();
      range.setStart(heading,0);
      var lastChildIndex = heading.childNodes.length;
      range.setEnd(heading,lastChildIndex);
      var text = range.toString();
      if (text)
      {
        // Use just first 40 characters, don't add "...",
        //  and replace whitespace with "_" and strip non-word characters
        text = ConvertToCDATAString(TruncateStringAtWordEnd(text, 40, false));
        // Append "_" to any name already in the list
        if (GetExistingHeadingIndex(text) > -1)
          text += "_";
        AppendStringToTreelist(gDialog.HeadingsList, text);

        // Save nodes in an array so we can create anchor node under it later
        if (!HNodeArray)
          HNodeArray = new Array(heading)
        else
          HNodeArray[HNodeArray.length] = heading;
      }
    }
  }
  if (HNodeArray)
  {
    gHaveHeadings = true;
  } else {
    // Message to tell user there are none
    item = AppendStringToTreelistById(gDialog.HeadingsList, "NoHeadings");
    if (item) item.setAttribute("disabled", "true");
  }
}

function doEnabling()
{
  // We disable Ok button when there's no href text only if inserting a new link
  var enable = insertNew ? (gDialog.hrefInput.value.trimString().length > 0) : true;

  SetElementEnabledById( "ok", enable);
}

var gClearListSelections = true;

function ChangeLocation()
{
  if (gClearListSelections)
  {
    // Unselect the treelists
    UnselectNamedAnchor();
    UnselectHeadings();
  }  

  SetRelativeCheckbox();

  // Set OK button enable state
  doEnabling();
}

function GetExistingHeadingIndex(text)
{
  var len = gDialog.HeadingsList.getAttribute("length");
  for (var i=0; i < len; i++)
  {
    if (GetTreelistValueAt(gDialog.HeadingsList, i) == text)
      return i;
  }
  return -1;
}

function SelectNamedAnchor()
{
  if (gCanChangeAnchorSelected)
  {
    if (gHaveNamedAnchors)
    {
      // Prevent ChangeLocation() from unselecting the list
      gClearListSelections = false;
      gDialog.hrefInput.value = "#"+GetSelectedTreelistValue(gDialog.NamedAnchorList);
      gClearListSelections = true;

      SetRelativeCheckbox();

      // ChangeLocation isn't always called, so be sure Ok is enabled
      doEnabling();
    }
    else
      UnselectNamedAnchor();
  
    UnselectHeadings();
  }
}

function SelectHeading()
{
  if (gCanChangeHeadingSelected)
  {
    if (gHaveHeadings)
    {
      gClearListSelections = false;
      gDialog.hrefInput.value = "#"+GetSelectedTreelistValue(gDialog.HeadingsList);
      gClearListSelections = true;

      SetRelativeCheckbox();
      doEnabling();
    }
    else
      UnselectHeadings();

    UnselectNamedAnchor();
  }
}

function UnselectNamedAnchor()
{
  // Prevent recursive calling of SelectNamedAnchor()
  gCanChangeAnchorSelected = false;
  gDialog.NamedAnchorList.selectedIndex = -1;  
  gCanChangeAnchorSelected = true;
}

function UnselectHeadings()
{
  // Prevent recursive calling of SelectHeading()
  gCanChangeHeadingSelected = false;
  gDialog.HeadingsList.selectedIndex = -1;  
  gCanChangeHeadingSelected = true;
}

// Get and validate data from widgets.
// Set attributes on globalElement so they can be accessed by AdvancedEdit()
function ValidateData()
{
  href = gDialog.hrefInput.value.trimString();
  if (href)
  {
    // Set the HREF directly on the editor document's anchor node
    //  or on the newly-created node if insertNew is true
    globalElement.setAttribute("href",href);
  }
  else if (insertNew)
  {
    // We must have a URL to insert a new link
    //NOTE: We accept an empty HREF on existing link to indicate removing the link
    ShowInputErrorMessage(GetString("EmptyHREFError"));
    return false;
  }
  if (gDialog.linkTextInput)
  {
    // The text we will insert isn't really an attribute,
    //  but it makes sense to validate it
    newLinkText = TrimString(gDialog.linkTextInput.value);
    if (!newLinkText)
    {
      if (href)
        newLinkText = href
      else
      {
        ShowInputErrorMessage(GetString("EmptyLinkTextError"));
        SetTextboxFocus(gDialog.linkTextInput);
        return false;
      }
    }
  }
  return true;
}

function doHelpButton()
{
  openHelp("chrome://help/content/help.xul?link_properties");
}

function onOK()
{
  if (ValidateData())
  {
    if (href.length > 0)
    {
      // Copy attributes to element we are changing or inserting
      editorShell.CloneAttributes(anchorElement, globalElement);

      // Coalesce into one undo transaction
      editorShell.BeginBatchChanges();

      // Get text to use for a new link
      if (insertLinkAtCaret)
      {
        // Append the link text as the last child node 
        //   of the anchor node
        var textNode = editorShell.editorDocument.createTextNode(newLinkText);
        if (textNode)
          anchorElement.appendChild(textNode);
        try {
          editorShell.InsertElementAtSelection(anchorElement, false);
        } catch (e) {
          dump("Exception occured in InsertElementAtSelection\n");
          return true;
        }
      } else if (insertNew || replaceExistingLink)
      {
        //  Link source was supplied by the selection,
        //  so insert a link node as parent of this
        //  (may be text, image, or other inline content)
        try {
          editorShell.InsertLinkAroundSelection(anchorElement);
        } catch (e) {
          dump("Exception occured in InsertElementAtSelection\n");
          return true;
        }
      }
      // Check if the link was to a heading 
      if (href[0] == "#")
      {
        var name = href.substr(1);
        var index = GetExistingHeadingIndex(name);
        if (index >= 0) {
          // We need to create a named anchor 
          //  and insert it as the first child of the heading element
          var headNode = HNodeArray[index];
          var anchorNode = editorShell.editorDocument.createElement("a");
          if (anchorNode) {
            anchorNode.name = name;
            // Remember to use editorShell method so it is undoable!
            editorShell.InsertElement(anchorNode, headNode, 0, false);
          }
        } else {
          dump("HREF is a heading but is not in the list!\n");
        }
      }
      editorShell.EndBatchChanges();
    } 
    else if (!insertNew)
    {
      // We already had a link, but empty HREF means remove it
      editorShell.RemoveTextProperty("a", "");
    }
    SaveWindowLocation();
    return true;
  }
  return false;
}
