var appCore;
var toolkitCore;
var insertNew = true;
var selectionIsCollapsed = false;
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
    appCore = XPAppCoresManager.Find("EditorAppCoreHTML");  
    if(!appCore || !toolkitCore) {
      dump("EditorAppCore not found!!!\n");
      toolkitCore.CloseWindow(window);
    }
    dump("EditorAppCore found for Image Properties dialog\n");
}
