// OnOK(), Undo(), and Cancel() are in EdDialogCommon.js
// applyChanges() must be implemented here

var appCore;
var toolkitCore;
var insertNew = true;
var undoCount = 0;

// dialog initialization code
function Startup()
{
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
  
  // temporary while this window is opend with ShowWindowWithArgs
  dump("Getting parent appcore\n");
  var editorName = document.getElementById("args").getAttribute("value");
  dump("Got editorAppCore called " + editorName + "\n");
  appCore = XPAppCoresManager.Find(editorName);  
  if(!appCore || !toolkitCore) {
    dump("EditorAppCore not found!!!\n");
    toolkitCore.CloseWindow(window);
    return;  
  }
  dump("EditorAppCore found for ???????? dialog\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // GET EACH CONTROL -- E.G.:
  //dialog.editBox = document.getElementById("editBox");

  initDialog();
  
  // SET FOCUS TO FIRST CONTROL
  //dialog.editBox.focus();
}

function initDialog() {
  // Get a single selected element of the desired type
  element = appCore.getSelectedElement(tagName);

  if (element) {
    // We found an element and don't need to insert one
    insertNew = false;
    dump("Found existing image\n");
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    element = appCore.createElementWithDefaults(tagName);
  }

  if(!element)
  {
    dump("Failed to get selected element or create a new one!\n");
    //toolkitCore.CloseWindow(window);
  }
}

function applyChanges()
{
// Set all attributes from the dialog values, for example:
  element.setAttribute("src",dialog.Src.value);

  if (insertNew) {
    appCore.insertElement(element, true)
  }
}