// OnOK(), Undo(), and Cancel() are in EdDialogCommon.js
// applyChanges() must be implemented here

var appCore;
var insertNew = true;
var imageWasInserted = false;
var undoCount = 0;
var imageElement;
var tagName = "img"
var advanced = false;

// dialog initialization code
function Startup()
{
  dump("Doing Startup...\n");
  
  // New metho: parameters passed via window.openDialog, which puts
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
  dump("EditorAppCore found for Image Properties dialog\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // This is the "combined" widget:
  dialog.srcInput = document.getElementById("image.srcInput");
  dialog.altTextInput = document.getElementById("image.altTextInput");

  dialog.AdvancedButton = document.getElementById("AdvancedButton");
  dialog.AdvancedRow = document.getElementById("AdvancedRow");
  
  // Start in "basic" mode
  dialog.AdvancedRow.style.display = "none";


  if (null == dialog.srcInput || 
      null == dialog.altTextInput )
  {
    dump("Not all dialog controls were found!!!\n");
  }
      
  initDialog();
  
  dialog.srcInput.focus();
}

function initDialog() {
  // Get a single selected anchor element
  imageElement = appCore.getSelectedElement(tagName);

  if (imageElement) {
    dump("Found existing image\n");
    // We found an element and don't need to insert one
    insertNew = false;

    // Set the controls to the image's attributes
    dialog.srcInput.value = imageElement.getAttribute("src");
    dialog.altTextInput.value = imageElement.getAttribute("alt");
  } else {
    insertNew = true;
    // We don't have an element selected, 
    //  so create one with default attributes
    dump("Element not selected - calling createElementWithDefaults\n");
    imageElement = appCore.createElementWithDefaults(tagName);
  }

  if(!imageElement) {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
}

function chooseFile()
{
  // Get a local file, converted into URL format
  fileName = appCore.getLocalFileURL(window, "img");
  if (fileName != "") {
    dialog.srcInput.value = fileName;
  }
  // Put focus into the input field
  dialog.srcInput.focus();
}

function onAdvanced()
{
  if (dialog.AdvancedRow) {
    dump("AdvancedRow still exists ****\n");
  }

  if (advanced) {
    dump("Changing to BASIC mode\n");
    advanced = false;
    // BUG: This works to hide the row, but
    //   setting visibility to "show" doesn't bring it back
    //dialog.AdvancedRow.style.visibility = "collapse";
    dialog.AdvancedRow.style.display = "none";
  } else {
    dump("Changing to ADVANCED mode\n");
    advanced = true;
    dialog.AdvancedRow.style.display = "table-row";
  }

}

function onOK() {
  if (applyChanges()) {
    if (imageWasInserted) {
      // We selected the object, undo it by
      //  setting caret to just after the inserted element
      appCore.setCaretAfterElement(imageElement);
    }
    window.close();
  }
}

function onCancel() {
  // Undo all actions performed within the dialog
  // TODO: We need to suppress reflow/redraw untill all levels are undone
  while (undoCount > 0) {
    onUndo();
  }
  window.close();
}

function applyChanges()
{
  // TODO: BE SURE Src AND AltText are completed!
  imageElement.setAttribute("src",dialog.srcInput.value);
  // We must convert to "file:///" format else image doesn't load!
  imageElement.setAttribute("alt",dialog.altTextInput.value);  
  if (insertNew) {
    dump("Src="+imageElement.getAttribute("src")+" Alt="+imageElement.getAttribute("alt")+"\n");
    appCore.insertElement(imageElement, true);
    // Select the newly-inserted image
    appCore.selectElement(imageElement);
    // Mark that we inserted so we can collapse the selection
    //  when dialog closes
    imageWasInserted = true;
  }
  // Reinitialize dialog data
  initDialog();

  // TODO: Return false if any data validation tests fail
  return true;
}