var editorShell;
var insertNew = true;
var imageWasInserted = false;
var imageElement;
var tagName = "img"
var advanced = true;

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
  dump("EditoreditorShell found for Image Properties dialog\n");

  // Create dialog object to store controls for easy access
  dialog = new Object;
  // This is the "combined" widget:
  dialog.srcInput = document.getElementById("image.srcInput");
  dialog.altTextInput = document.getElementById("image.altTextInput");

  dialog.AdvancedButton = document.getElementById("AdvancedButton");
  dialog.AdvancedRow = document.getElementById("AdvancedRow");
  
  // Start in the mode initialized in the "advanced" var above
  // THIS IS NOT WORKING NOW - After switching to "basic" mode,
  // then back to 
  if (advanced) {
    dialog.AdvancedRow.style.visibility = "visible";
  } else {
    dialog.AdvancedRow.style.visibility = "collapse";
  }

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
  imageElement = editorShell.GetSelectedElement(tagName);

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
    imageElement = editorShell.CreateElementWithDefaults(tagName);
  }

  if(!imageElement) {
    dump("Failed to get selected element or create a new one!\n");
    window.close();
  }
}

function chooseFile()
{
  // Get a local file, converted into URL format
  fileName = editorShell.GetLocalFileURL(window, "img");
  if (fileName != "") {
    dialog.srcInput.value = fileName;
  }
  // Put focus into the input field
  dialog.srcInput.focus();
}

function onAdvanced()
{
  if (advanced) {
    dump("Changing to BASIC mode\n");
    advanced = false;
    // BUG: This works to hide the row, but
    //   setting visibility to "show" doesn't bring it back
    dialog.AdvancedRow.style.visibility = "collapse";
    //dialog.AdvancedRow.style.display = "none";
  } else {
    dump("Changing to ADVANCED mode\n");
    advanced = true;
    //dialog.AdvancedRow.style.display = "table-row";
    dialog.AdvancedRow.style.visibility = "visible";
  }

}

function SelectWidthUnits()
{
   list = document.getElementByID("WidthUnits");
   value = list.options[list.selectedIndex].value;
   dump("Selected item: "+value+"\n");
}

function OnChangeSrc()
{
  dump("*** Changed SRC field\n");
}

function onOK()
{
  // TODO: BE SURE Src AND AltText are completed!

  imageElement.setAttribute("src",dialog.srcInput.value);
  // We must convert to "file:///" format else image doesn't load!
  imageElement.setAttribute("alt",dialog.altTextInput.value);  
  if (insertNew) {
    dump("Src="+imageElement.getAttribute("src")+" Alt="+imageElement.getAttribute("alt")+"\n");
    editorShell.InsertElement(imageElement, true);
    // Select the newly-inserted image
    editorShell.SelectElement(imageElement);
    // Mark that we inserted so we can collapse the selection
    //  when dialog closes
    imageWasInserted = true;
  }
  if (imageWasInserted) {
    // We selected the object, undo it by
    //  setting caret to just after the inserted element
    editorShell.SetSelectionAfterElement(imageElement);
  }
  window.close();
}
