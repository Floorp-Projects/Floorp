var appCore;
var toolkitCore;
var anchorElement = null;
var insertNew = true;
var needLinkText = false;
var selectionIsCollapsed = false;
var undoCount = 0;

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
  dialog.linkMessage      = document.getElementById("linkMessage");
  dialog.ok               = document.getElementById("OK");

  if (null == dialog.linkTextInput || 
      null == dialog.hrefInput ||
      null == dialog.linkMessage ||
      null == dialog.ok )
  {
    dump("Not all dialog controls were found!!!\n");
  }

  if (!insertNew)
  {
    var parent = dialog.linkTextInput.parentNode;
    if (parent) {
      parent.removeChild(dialog.linkTextInput);
      dialog.linkTextInput = null;
      // TODO: Replace the text with the currently-selected text
    }
  }
  initDialog();
}

function initDialog() {
  // Get a single selected anchor element
  anchorElement = appCore.getSelectedElement(tagName);

  selection = appCore.editorSelection;
  if (selection)
  {
    selectionIsCollapsed = selection.selectionIsCollapsed;
    dump("There is a selection: collapsed = "+selectionIsCollapsed+"\n");
  } else {
    dump("Failed to get selection\n");
  }

  if (anchorElement) {
    // We found an element and don't need to insert one
    insertNew = false;

    // BUT href is a weird case: If selection extends beyond
    //   the link, user probably wants to extend link to 
    //   entire selection. We do this by "inserting" the link
    //   (actually does the appropriate reparenting)
    if (!selectionIsCollapsed)
    {
      insertNew = true;
    }
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    anchorElement = appCore.createElementWithDefaults(tagName);
  }

  if(!anchorElement)
  {
    dump("Failed to get selected element or create a new one!\n");
    toolkitCore.CloseWindow(window);
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
  if (insertNew)
  {
    // Append the link text as the last child node 
    //   of the anchor node
    textNode = appCore.editorDocument.createTextNode(dialog.linkTextInput.value);
    if (textNode)
    {
      anchorElement.appendChild(textNode);
    }
    newElement = appCore.insertElement(anchorElement, true);
    if (newElement != anchorElement)
    {
      dump("Returned element from insertElement is different from orginal element.\n");
    }
  }
  undoCount = undoCount + 1;
  appCore.endBatchChanges();

  // Reinitialize dialog data
  initDialog();
}

function onUndo() {
  if (undoCount > 0)
  {
    dump("Undo count = "+undoCount+"\n");
    undoCount = undoCount - 1;
    appCore.undo();
  }
}

function onOK() {
  applyChanges();
  //toolkitCore.CloseWindow(window);
}

function onCancel() {
  // Undo all actions performed within the dialog
  // TODO: We need to suppress reflow/redraw untill all levels are undone
  while (undoCount > 0) {
    appCore.undo();
  }
  //toolkitCore.CloseWindow(window);
}

