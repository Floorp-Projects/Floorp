//Cancel() is in EdDialogCommon.js
var editorShell;
var insertNew = true;
var inserted = false;
var tagname = "TAG NAME"

// dialog initialization code
function Startup()
{
  // get the editor shell from the parent window
  editorShell = window.opener.editorShell;
  editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
  if(!editorShell) {
    dump("EditoreditorShell not found!!!\n");
    window.close();
    return;
  }
  dump("EditoreditorShell found for NamedAnchor Properties dialog\n");

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
function CreatePixelOrPercentMenu()
{
  dump("Creating PixelOrPercent popup menu\n");
}

function onOK()
{
// Set attribute example:
//  imageElement.setAttribute("src",dialog.srcInput.value);
  if (insertNew) {
    editorShell.InsertElement(element, true);
    // Select the newly-inserted image
    editorShell.SelectElement(element);
    // Mark that we inserted so we can collapse the selection
    //  when dialog closes
    inserted = true;
  }

  if (inserted) {
    // We selected the object, undo it by
    //  setting caret to just after the inserted element
    editorShell.SetSelectionAfterElement(imageElement);
  }
  window.close();
}
