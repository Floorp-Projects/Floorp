// OnOK(), Undo(), and Cancel() are in EdDialogCommon.js
// applyChanges() must be implemented here

var appCore;
var toolkitCore;
var anchorElement = null;
var insertNew = true;
var needLinkText = false;
var selection;
var undoCount = 0;
var makeLinkWithSelection = false;

// NOTE: Use "HREF" instead of "A" to distinguish from Named Anchor
// The returned node is has an "A" tagName
var tagName = "HREF";
var data;
var dialog;

// dialog initialization code
function Startup() {
  dump("Doing Startup...\n");
  toolkitCore = XPAppCoresManager.Find("ToolkitCore");
  if (!toolkitCore) {
    toolkitCore = new ToolkitCore();
    if (toolkitCore)
      toolkitCore.Init("ToolkitCore");
  }
  if(!toolkitCore) {
    dump("toolkitCore not found!!! And we can't close the dialog!\n");
  }

  // NEVER create an appcore here - we must find parent editor's
  appCore = XPAppCoresManager.Find("EditorAppCoreHTML");  
  if(!appCore || !toolkitCore) {
    dump("EditorAppCore not found!!!\n");
    toolkitCore.CloseWindow(window);
  }
  dump("EditorAppCore found for Link Properties dialog\n");
  
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
    dialog.linkTextInput.focus();
  } else {
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

function initDialog() {
  // Get a single selected anchor element
  anchorElement = appCore.getSelectedElement(tagName);

  selection = appCore.editorSelection;
  if (selection) {
    dump("There is a selection: collapsed = "+selection.isCollapsed+"\n");
  } else {
    dump("Failed to get selection\n");
  }

  if (anchorElement) {
    // We found an element and don't need to insert one
    insertNew = false;

    if (!selection.isCollapsed)
    {
      // HREF is a weird case: If selection extends beyond
      //   the link, user probably wants to extend link to 
      //   entire selection.
      // TODO: We need to know if selection extends beyond existing
      //   link text before we should do this
      //makeLinkWithSelection = true;
    }
  } else {
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    anchorElement = appCore.createElementWithDefaults(tagName);

    // We will insert a new link at caret location if there's no selection
    // TODO: This isn't entirely correct. If selection doesn't have any text
    //   or an image, then shouldn't we clear the selection and insert new text?
    insertNew = selection.isCollapsed;
  }
  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    toolkitCore.CloseWindow(window);
  } else if (!insertNew) {
      dump("Need to get selected text\n");

    // Replace the link message with the link source string
    // TODO: Get the text of the selection WHAT ABOUT IMAGES?
    //  Maybe have a special method "GetLinkSource" that resolves images as
    //   their URL? E.g.: "Link source [image:http://myimage.gif]"
    dialog.linkMessage.data = "[Link source text or image URL goes here]";
  }
}

function applyChanges()
{
  // Coalesce into one undo transaction
  appCore.beginBatchChanges();

  // Set the HREF directly on the editor document's anchor node
  //  or on the newly-created node if insertNew is true
  anchorElement.setAttribute("href",dialog.hrefInput.value);

  // Get text to use for a new link
  if (insertNew) {
    // Append the link text as the last child node 
    //   of the anchor node
    textNode = appCore.editorDocument.createTextNode(dialog.linkTextInput.value);
    if (textNode) {
      anchorElement.appendChild(textNode);
    }
    newElement = appCore.insertElement(anchorElement, true);
    if (newElement != anchorElement) {
      dump("Returned element from insertElement is different from orginal element.\n");
    }
  }
  undoCount = undoCount + 1;
  appCore.endBatchChanges();

  // Reinitialize dialog data
  initDialog();
}
