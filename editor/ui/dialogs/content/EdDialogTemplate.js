//Cancel() is in EdDialogCommon.js
var insertNew = true;
var tagname = "TAG NAME"

// dialog initialization code
function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditoreditorShell found for dialog\n");

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
  element = editorShell.GetSelectedElement(tagName);

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
    window.close();
  }
}

function onOK()
{
// Set attribute example:
//  imageElement.setAttribute("src",dialog.srcInput.value);
  if (insertNew) {
    editorShell.InsertElement(element, false);
  }
  window.close();
}
