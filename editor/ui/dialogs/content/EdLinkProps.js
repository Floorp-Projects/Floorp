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
var href;
var newLinkText;
var NamedAnchorList = 0;
var HNodeArray;
var haveNamedAnchors = false;
var haveHeadings = false;
var MoreSection;
var MoreFewerButton;
var SeeMore = false;
var AdvancedEditSection;

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
  linkCaption     = (document.getElementById("linkTextCaption")).firstChild;
  linkMessage     = (document.getElementById("linkTextMessage")).firstChild;
  linkTextInput   = document.getElementById("linkTextInput");
  hrefInput       = document.getElementById("hrefInput");
  NamedAnchorList = document.getElementById("NamedAnchorList");
  HeadingsList    = document.getElementById("HeadingsList");
  MoreSection     = document.getElementById("MoreSection");
  MoreFewerButton  = document.getElementById("MoreFewerButton");
  AdvancedEditSection = document.getElementById("AdvancedEditButton");

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
        linkCaption.data = GetString("LinkImage");
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
      linkCaption.data = GetString("EnterLinkText");
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
    linkCaption.data = GetString("LinkText");
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

  // Make a copy to use for AdvancedEdit and onSaveDefault
  globalElement = anchorElement.cloneNode(false);

  // Get the list of existing named anchors and headings
  FillListboxes();

  // Set data for the dialog controls
  InitDialog();

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
  // Set SeeMore bool to the OPPOSITE of the current state,
  //   which is automatically saved by using the 'persist="more"' 
  //   attribute on the MoreFewerButton button
  //   onMoreFewer will toggle it and redraw the dialog
  SeeMore = (MoreFewerButton.getAttribute("more") != "1");
  onMoreFewer();
}

// Set dialog widgets with attribute data
// We get them from globalElement copy so this can be used
//   by AdvancedEdit(), which is shared by all property dialogs
function InitDialog()
{
  hrefInput.value = globalElement.getAttribute("href");
  dump("Current HREF: "+hrefInput.value+"\n");
}

function ChooseFile()
{
  // Get a local file, converted into URL format
  fileName = editorShell.GetLocalFileURL(window, "html");
  if (fileName) {
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

function FillListboxes()
{
  NamedAnchorNodeList = editorShell.editorDocument.anchors;
  var NamedAnchorCount = NamedAnchorNodeList.length;
  if (NamedAnchorCount > 0) {
    for (var i = 0; i < NamedAnchorCount; i++) {
      AppendStringToList(NamedAnchorList,NamedAnchorNodeList.item(i).name);
    }
    haveNamedAnchors = true;
  } else {
    // Message to tell user there are none
    AppendStringToList(NamedAnchorList,GetString("NoNamedAnchors"));
    NamedAnchorList.setAttribute("disabled", "true");
  }
  var firstHeading = true;
  for (var j = 1; j <= 6; j++) {
    var headingList = editorShell.editorDocument.getElementsByTagName("h"+String(j));
    dump(headingList+" Count= "+headingList.length+"\n");
    if (headingList.length > 0) {
      dump("HELLO\n");
      var heading = headingList.item(0);

      // Skip headings that already have a named anchor as their first child
      //  (this may miss nearby anchors, but at least we don't insert another
      //   under the same heading)
      var child = heading.firstChild;
      dump(child.name+" = Child.name. Length="+child.name.length+"\n");
      if (child && child.nodeName == "A" && child.name && (child.name.length>0)) {
        continue;
      }

      var range = editorShell.editorDocument.createRange();
      range.setStart(heading,0);
      var lastChildIndex = heading.childNodes.length;
      range.setEnd(heading,lastChildIndex);
      var text = range.toString();
      if (text) {
        // Use just first 40 characters, don't add "...",
        //  and replace whitespace with "_" and strip non-word characters
        text = PrepareStringForURL(TruncateStringAtWordEnd(text, 40, false));
        // Append "_" to any name already in the list
        if (GetExistingHeadingIndex(text) > -1)
          text += "_";
        AppendStringToList(HeadingsList, text);

        // Save nodes in an array so we can create anchor node under it later
        if (!HNodeArray)
          HNodeArray = new Array(heading)
        else
          HNodeArray[HNodeArray.length] = heading;
      }
    }
  }
  if (HNodeArray) {
    haveHeadings = true;
  } else {
    // Message to tell user there are none
    AppendStringToList(HeadingsList,GetString("NoHeadings"));
    HeadingsList.setAttribute("disabled", "true");
  }
}

function GetExistingHeadingIndex(text)
{
  dump("Heading text: "+text+"\n");
  for (i=0; i < HeadingsList.length; i++) {
    dump("HeadingListItem"+i+": "+HeadingsList.options[i].value+"\n");
    if (HeadingsList.options[i].value == text)
      return i;
  }
  return -1;
}

function SelectNamedAnchor()
{
  if (haveNamedAnchors) {
    hrefInput.value = "#"+NamedAnchorList.options[NamedAnchorList.selectedIndex].value;
  }
}

function SelectHeading()
{
  if (haveHeadings) {
    hrefInput.value = "#"+HeadingsList.options[HeadingsList.selectedIndex].value;
  }
}

function onMoreFewer()
{
  if (SeeMore)
  {
    MoreSection.setAttribute("style","display: none");
    window.sizeToContent();
    MoreFewerButton.setAttribute("more","0");
    MoreFewerButton.setAttribute("value",GetString("MoreProperties"));
    SeeMore = false;
  }
  else
  {
    MoreSection.setAttribute("style","display: inherit");
    window.sizeToContent();
    MoreFewerButton.setAttribute("more","1");
    MoreFewerButton.setAttribute("value",GetString("FewerProperties"));
    SeeMore = true;
  }
}


// Get and validate data from widgets.
// Set attributes on globalElement so they can be accessed by AdvancedEdit()
function ValidateData()
{
  href = hrefInput.value.trimString();
  if (href.length > 0) {
    // Set the HREF directly on the editor document's anchor node
    //  or on the newly-created node if insertNew is true
    globalElement.setAttribute("href",href);
    dump("HREF:"+href+"|\n");
  } else if (insertNew) {
    // We must have a URL to insert a new link
    //NOTE: WE ACCEPT AN EMPTY HREF TO ALLOW REMOVING AN EXISTING LINK,
    dump("Empty HREF error\n");
    ShowInputErrorMessage(GetString("EmptyHREFError"));
    return false;
  }
  if (linkTextInput) {
    // The text we will insert isn't really an attribute,
    //  but it makes sense to validate it
    newLinkText = TrimString(linkTextInput.value);
    if (newLinkText.length == 0) {
      ShowInputErrorMessage(GetString("GetInputError"));
      linkTextInput.focus();
      return false;
    }
  }
  return true;
}


function onOK()
{
  if (ValidateData())
  {
    if (href.length > 0) {
      // Copy attributes to element we are changing or inserting
      editorShell.CloneAttributes(anchorElement, globalElement);

      // Coalesce into one undo transaction
      editorShell.BeginBatchChanges();

      // Get text to use for a new link
      if (insertNew) {
        // Append the link text as the last child node 
        //   of the anchor node
        textNode = editorShell.editorDocument.createTextNode(newLinkText);
        if (textNode) {
          anchorElement.appendChild(textNode);
        }
        try {
          editorShell.InsertElementAtSelection(anchorElement, false);
        } catch (e) {
          dump("Exception occured in InsertElementAtSelection\n");
          return true;
        }
      } else if (insertLinkAroundSelection) {
        // Text was supplied by the selection,
        //  so insert a link node as parent of this text
        try {
          editorShell.InsertLinkAroundSelection(anchorElement);
        } catch (e) {
          dump("Exception occured in InsertElementAtSelection\n");
          return true;
        }
      }
      // Check if the link was to a heading 
      if (href[0] == "#") {
        var name = href.substr(1);
        var index = GetExistingHeadingIndex(name);
        dump("Heading name="+name+" Index="+index+"\n");
        if (index >= 0) {
          // We need to create a named anchor 
          //  and insert it as the first child of the heading element
          var headNode = HNodeArray[index];
          var anchorNode = editorShell.editorDocument.createElement("a");
          if (anchorNode) {
            anchorNode.name = name;
            // Remember to use editorShell method so it is undoable!
            editorShell.InsertElement(anchorNode, headNode, 0);
            dump("Anchor node created and inserted under heading\n");
          }
        } else {
          dump("HREF is a heading but is not in the list!\n");
        }
      }
      editorShell.EndBatchChanges();
    } else if (!insertNew) {
      // We already had a link, but empty HREF means remove it
      editorShell.RemoveTextProperty("a", "");
    }
    return true;
  }
  return false;
}
