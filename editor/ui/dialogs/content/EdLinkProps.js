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
var insertNew = true;
var needLinkText = false;
var insertLinkAroundSelection = false;
var linkTextInput;
var hrefInput;
var linkMessage;

// NOTE: Use "href" instead of "a" to distinguish from Named Anchor
// The returned node is has an "a" tagName
var tagName = "href";

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;

  doSetOKCancel(onOK, null);
  
  // Message was wrapped in a <label> or <div>, so actual text is a child text node
  linkCaption      = (document.getElementById("linkTextCaption")).firstChild;
  linkMessage      = (document.getElementById("linkTextMessage")).firstChild;
  linkTextInput    = document.getElementById("linkTextInput");
  hrefInput        = document.getElementById("hrefInput");

  if (!linkTextInput || 
      !hrefInput ||
      !linkMessage ||
      !linkCaption)
  {
    dump("Not all dialog controls were found!!!\n");
  }
  
  // Set data for the dialog controls
  initDialog();

  // Set initial focus

  if (insertNew) {
    dump("Setting focus to linkTextInput\n");
    // We will be using the HREF inputbox, so text message
    linkTextInput.focus();
  } else {
    dump("Setting focus to linkTextInput\n");
    hrefInput.focus();

    // We will not insert a new link at caret, so remove link text input field
    parentNode = linkTextInput.parentNode;
    if (parentNode) {
      dump("Removing link text input field.\n");
      parentNode.removeChild(linkTextInput);
      linkTextInput = null;
    }
  }
}

function initDialog()
{
  // Get a single selected anchor element
  anchorElement = editorShell.GetSelectedElement(tagName);

  var selection = editorShell.editorSelection;
  if (selection) {
    dump("There is a selection: collapsed = "+selection.isCollapsed+"\n");
  } else {
    dump("Failed to get selection\n");
  }

  if (anchorElement) {
    // We found an element and don't need to insert one
    dump("found anchor element\n");
    insertNew = false;

    // We get the anchor if any of the selection (or just caret)
    //  is enclosed by the link. Select the entire link
    //  so we can show the selection text
    editorShell.SelectElement(anchorElement);
    selection = editorShell.editorSelection;

    hrefInput.value = anchorElement.getAttribute("href");
    dump("Current HREF: "+hrefInput.value+"\n");
  } else {
    // See if we have a selected image instead of text
    imageElement = editorShell.GetSelectedElement("img");
    if (imageElement) {
      // See if the image is a child of a link
      dump("Image element found - check if its a link...\n");
      dump("Image Parent="+parent);
      parent = imageElement.parentNode;
        dump("Parent="+parent+" nodeName="+parent.nodeName+"\n");
      if (parent) {
        anchorElement = parent;
        insertNew = false;
        // GET THIS FROM STRING BUNDLE
        linkCaption.data = "Link image:"
        // Link source string is the source URL of image
        // TODO: THIS STILL DOESN'T HANDLE MULTIPLE SELECTED IMAGES!
        linkMessage.data = imageElement.getAttribute("src");;
      }
    } else {
      // We don't have an element selected, 
      //  so create one with default attributes
      dump("Element not selected - calling createElementWithDefaults\n");
      anchorElement = editorShell.CreateElementWithDefaults(tagName);

      // We will insert a new link at caret location if there's no selection
      // TODO: This isn't entirely correct. If selection doesn't have any text
      //   or an image, then shouldn't we clear the selection and insert new text?
      insertNew = selection.isCollapsed;
      dump("insertNew is " + insertNew + "\n");
      linkCaption.data = "Enter text for the link:"
      linkMessage.data = "";
    }
  }
  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  } else if (!insertNew && !imageElement) {

    // Replace the link message with the link source string
    selectedText = GetSelectionAsText();
    if (selectedText.length > 0) {
      // Use just the first 50 characters and add "..."
      selectedText = TruncateStringAtWordEnd(selectedText, 50, true);
    } else {
      dump("Selected text for link source not found. Non-text elements selected?\n");
    }
    linkMessage.data = selectedText;
    // The label above the selected text:
    linkCaption.data = "Link text:"
  }

  if (!selection.isCollapsed)
  {
    // HREF is a weird case: If selection extends beyond
    //   the link, user probably wants to extend link to 
    //   entire selection.
    // TODO: If there was already a link, 
    //   we need to know if selection extends beyond existing
    //   link text before we should do this
    insertLinkAroundSelection = true;
    dump("insertLinkAroundSelection is TRUE\n");
  }
}

function ChooseFile()
{
  // Get a local file, converted into URL format
  fileName = editorShell.GetLocalFileURL(window, "html");
  if (StringExists(fileName)) {
    hrefInput.value = fileName;
  }
  // Put focus into the input field
  hrefInput.focus();
}

function RemoveLink()
{
  // Simple clear the input field!
  hrefInput.value = "";
}

function onOK()
{
  // TODO: VALIDATE FIELDS BEFORE COMMITING CHANGES

  href = TrimString(hrefInput.value);
  if (href.length > 0) {
    // Coalesce into one undo transaction
    editorShell.BeginBatchChanges();

    // Set the HREF directly on the editor document's anchor node
    //  or on the newly-created node if insertNew is true
    anchorElement.setAttribute("href",href);

    // Get text to use for a new link
    if (insertNew) {
      // Append the link text as the last child node 
      //   of the anchor node
      dump("Creating text node\n");
      newText = TrimString(linkTextInput.value);
      if (newText.length == 0) {
        ShowInputErrorMessage("You must enter some text for this link.");
        linkTextInput.focus();
        return false;
      }
      textNode = editorShell.editorDocument.createTextNode(newText);
      if (textNode) {
        anchorElement.appendChild(textNode);
      }
      dump("Inserting\n");
      editorShell.InsertElement(anchorElement, false);
    } else if (insertLinkAroundSelection) {
      // Text was supplied by the selection,
      //  so insert a link node as parent of this text
      dump("Setting link around selected text\n");
      editorShell.InsertLinkAroundSelection(anchorElement);
    }
    editorShell.EndBatchChanges();
  } else if (!insertNew) {
    // We already had a link, but empty HREF means remove it
    editorShell.RemoveTextProperty("a", "");
  }
  // Note: if HREF is empty and we were inserting a new link, do nothing
  
  return true;
}

