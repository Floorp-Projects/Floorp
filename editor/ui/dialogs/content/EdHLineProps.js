var editorShell;
var toolkitCore;
var insertNew = true;

// dialog initialization code
function Startup()
{
  dump("Doing Startup...\n");

  // get the editor shell from the parent window
  editorShell = window.opener.editorShell;
  editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
  if(!editorShell) {
    dump("EditoreditorShell not found!!!\n");
    window.close();
    return;
  }

  dump("EditoreditorShell found for HRule Properties dialog\n");

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
