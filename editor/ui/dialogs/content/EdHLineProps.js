var appCore;
var toolkitCore;
var insertNew = true;

// dialog initialization code
function Startup()
{
  dump("Doing Startup...\n");
  // New method: parameters passed via window.openDialog, which puts
  //  arguments in the array "arguments"
  var editorName = window.arguments[0];
  dump("Got editorAppCore called " + editorName + "\n");
  
  // NEVER create an appcore here - we must find parent editor's
  appCore = XPAppCoresManager.Find(editorName);  
  if(!appCore) {
    dump("EditorAppCore not found!!!\n");
    window.close();
    return;
  }
  dump("EditorAppCore found for HRule Properties dialog\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // GET EACH CONTROL -- E.G.:
  //dialog.editBox = document.getElementById("editBox");
  // Can we get at just the edit field?
  dialog.AltText = document.getElementById("image.AltText");

  //initDialog();
  
  // SET FOCUS TO FIRST CONTROL
  //dialog.editBox.focus();
}

function OnOK()
{
}
