var anchorElement = null;
var imageElement = null;
var insertNew = true;
var needLinkText = false;
var selection;
var insertLinkAroundSelection = false;

// NOTE: Use "href" instead of "a" to distinguish from Named Anchor
// The returned node is has an "a" tagName
var tagName = "href";
var dialog;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  
  // Create dialog object to store controls for easy access
  dialog = new Object;
  dialog.linkTextInput    = document.getElementById("linkTextInput");
  dialog.hrefInput        = document.getElementById("hrefInput");

  // Kinda clunky: Message was wrapped in a <p>, so actual message is a child text node
  dialog.linkMessage      = (document.getElementById("linkMessage")).firstChild;

  if (null == dialog.linkTextInput || 
      null == dialog.hrefInput ||
      null == dialog.linkMessage )
  {
    dump("Not all dialog controls were found!!!\n");
  }
  
  // Set data for the dialog controls
  initDialog();

  // Set initial focus

  if (insertNew) {
    dump("Setting focus to linkTextInput\n");
    dialog.linkTextInput.focus();
  } else {
    dump("Setting focus to linkTextInput\n");
    dialog.hrefInput.focus();

    // We will not insert a new link at caret, so remove link text input field
    parentNode = dialog.linkTextInput.parentNode;
    if (parentNode) {
      dump("Removing link text input field.\n");
      parentNode.removeChild(dialog.linkTextInput);
      dialog.linkTextInput = null;
    }
  }
}

function initDialog()
{
  // Get a single selected anchor element
  anchorElement = editorShell.GetSelectedElement(tagName);

  selection = editorShell.editorSelection;
  if (selection) {
    dump("There is a selection: collapsed = "+selection.isCollapsed+"\n");
  } else {
    dump("Failed to get selection\n");
  }

  if (anchorElement) {
    dump("found anchor element\n");
    // We found an element and don't need to insert one
    insertNew = false;
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
        // Link source string is the source URL of image
        // TODO: THIS STILL DOESN'T HANDLE MULTIPLE SELECTED IMAGES!
        dialog.linkMessage.data = imageElement.getAttribute("src");;
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
    }
  }
  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  } else if (!insertNew && !imageElement) {

    // Replace the link message with the link source string
    selectedText = editorShell.selectionAsText;
    if (selectedText.length > 0) {
      // Use just the first 50 characters and add "..."
      selectedText = TruncateStringAtWordEnd(selectedText, 50, true);
    } else {
      dump("Selected text for link source not found. Non-text elements selected?\n");
    }
    dialog.linkMessage.data = selectedText;
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

function chooseFile()
{
  // Get a local file, converted into URL format
  fileName = editorShell.GetLocalFileURL(window, "html");
  if (fileName != "") {
    dialog.hrefInput.value = fileName;
  }
  // Put focus into the input field
  dialog.hrefInput.focus();
}

function onOK()
{
  // TODO: VALIDATE FIELDS BEFORE COMMITING CHANGES

  // Coalesce into one undo transaction
  editorShell.BeginBatchChanges();

  // Set the HREF directly on the editor document's anchor node
  //  or on the newly-created node if insertNew is true
  anchorElement.setAttribute("href",dialog.hrefInput.value);

  // Get text to use for a new link
  if (insertNew) {
    // Append the link text as the last child node 
    //   of the anchor node
    dump("Creating text node\n");
    textNode = editorShell.editorDocument.createTextNode(dialog.linkTextInput.value);
    if (textNode) {
      anchorElement.appendChild(textNode);
    }
    dump("Inserting\n");
    editorShell.InsertElement(anchorElement, false);
  } else if (insertLinkAroundSelection) {
    dump("Setting link around selected text\n");
    editorShell.InsertLinkAroundSelection(anchorElement);
  }
  editorShell.EndBatchChanges();

  window.close();
}

