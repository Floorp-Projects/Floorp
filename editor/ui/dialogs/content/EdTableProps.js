//Cancel() is in EdDialogCommon.js
var tagname = "table"
var element;

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditoreditorShell found for dialog Table Properties\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // GET EACH CONTROL -- E.G.:
  //dialog.editBox = document.getElementById("editBox");

  initDialog();
  
  var table = editorShell.GetElementOrParentByTagName("table", null);
  if (!table)
    window.close();

  // SET FOCUS TO FIRST CONTROL
  //dialog.editBox.focus();

}

function initDialog() {
/*
  if(!element)
  {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
*/
}

function onOK()
{
// Set attribute example:
//  imageElement.setAttribute("src",dialog.srcInput.value);
  window.close();
}
