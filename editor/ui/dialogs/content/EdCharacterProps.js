// OnOK(), Undo(), and Cancel() are in EdDialogCommon.js
// applyChanges() must be implemented here

// dialog initialization code
var appCore;
var toolkitCore;
var insertNew = true;
var selectionIsCollapsed = false;
var undoCount = 0;

function Startup()
{
    dump("Doing Character Props Startup...\n");
    toolkitCore = XPAppCoresManager.Find("ToolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore)
        toolkitCore.Init("ToolkitCore");
    }
    if(!toolkitCore) {
      dump("toolkitCore not found!!! And we can't close the dialog!\n");
    }

  // temporary while this window is opend with ShowWindowWithArgs
  dump("Getting parent appcore\n");
  var editorName = document.getElementById("args").getAttribute("value");
  dump("Got editorAppCore called " + editorName + "\n");

    // NEVER create an appcore here - we must find parent editor's
    appCore = XPAppCoresManager.Find(editorName);  
    if(!appCore || !toolkitCore) {
      dump("EditorAppCore not found!!!\n");
      toolkitCore.CloseWindow(window);
    }
}

function applyChanges()
{
}
